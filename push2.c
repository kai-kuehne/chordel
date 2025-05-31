// https://github.com/Ableton/push-interface

#include "push2.h"
#include "log.h"

static SDL_mutex* mut;

static inline pixel_t bgr565_from_rgb(const Uint8* rgb) {
    Uint8 r = rgb[0];
    Uint8 g = rgb[1];
    Uint8 b = rgb[2];

    Uint16 bgr565 = (Uint16)((b & 0xf8) << 8); // Blue: top 5 bits
    bgr565 |= (Uint16)((g & 0xfc) << 3);       // Green: top 6 bits
    bgr565 |= (Uint16)((r & 0xf8) >> 3);       // Red: top 5 bits

    return bgr565;
}

static void push_display_init(PushDisplay* display) {
    SDL_memset(display->data_source, 0, PUSH_DISPLAY_WIDTH * PUSH_DISPLAY_HEIGHT * sizeof(pixel_t));
    SDL_memset(display->bitmap, 0, PUSH_DISPLAY_BITMAP_WIDTH * PUSH_DISPLAY_BITMAP_HEIGHT * sizeof(pixel_t));
}

static void push_display_flip(PushDisplay* display) {
    SDL_assert(PUSH_DISPLAY_HEIGHT == PUSH_DISPLAY_BITMAP_HEIGHT);

    const pixel_t* src      = display->bitmap;
    pixel_t*       dst      = display->data_source;
    const size_t   row_size = PUSH_DISPLAY_BITMAP_WIDTH * sizeof(pixel_t);

    SDL_LockMutex(mut);
    for (Uint8 line = 0; line < PUSH_DISPLAY_HEIGHT; line++) {
        SDL_memcpy(dst, src, row_size);
        src += PUSH_DISPLAY_BITMAP_WIDTH;
        dst += PUSH_DISPLAY_WIDTH;
    }
    SDL_UnlockMutex(mut);
}

static int communicator_submit_transfer(UsbCommunicator* comm, struct libusb_transfer* transfer) {
    SDL_assert(comm != NULL);
    SDL_assert(transfer != NULL);

    if (SDL_AtomicGet(&comm->terminate) == SDL_TRUE) {
        return -1;
    }

    SDL_LockMutex(mut);
    int result = libusb_submit_transfer(transfer);
    SDL_UnlockMutex(mut);

    if (result == 0) {
        SDL_AtomicIncRef(&comm->transfers_in_flight);
    } else {
#if DEBUG
        SDL_TriggerBreakpoint();
#endif
    }

    return result;
}

static void communicator_send_next_slice(UsbCommunicator* comm, struct libusb_transfer* transfer) {
    SDL_assert(comm != NULL);
    SDL_assert(transfer != NULL);

    if (comm->current_line == 0 && communicator_submit_transfer(comm, comm->transfers[0]) < 0) {
        LOG_WARN("libusb: could not submit frame header transfer");
        return;
    }

    SDL_LockMutex(mut);
    SDL_memcpy(
        transfer->buffer, (const unsigned char*)comm->data_source + LINE_SIZE * comm->current_line, SEND_BUFFER_SIZE
    );
    SDL_UnlockMutex(mut);

    if (communicator_submit_transfer(comm, transfer) < 0) {
        LOG_WARN("libusb: could not submit display data transfer");
        return;
    }

    SDL_LockMutex(mut);
    comm->current_line = (Uint8)(comm->current_line + LINE_COUNT_PER_SEND_BUFFER) % PUSH_DISPLAY_HEIGHT;
    SDL_UnlockMutex(mut);
}

static void communicator_transfer_callback(struct libusb_transfer* transfer) {
    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        if (transfer->length != transfer->actual_length) {
            LOG_WARN("libusb: only transferred %d of %d bytes", transfer->actual_length, transfer->length);
#if DEBUG
            SDL_TriggerBreakpoint();
#endif
            return;
        }
        UsbCommunicator* comm = (UsbCommunicator*)transfer->user_data;
        (void)SDL_AtomicDecRef(&comm->transfers_in_flight);
        if (transfer == comm->transfers[0]) {
            if (comm->render_callback != NULL) {
                comm->render_callback(
                    &comm->render_pixels, &comm->render_width, &comm->render_height, &comm->render_stride
                );
            }
        } else {
            communicator_send_next_slice(comm, transfer);
        }
    } else {
        switch (transfer->status) {
            case LIBUSB_TRANSFER_ERROR:
                LOG_WARN("libusb: Transfer failed");
                break;
            case LIBUSB_TRANSFER_TIMED_OUT:
                LOG_WARN("libusb: Transfer timed out");
                break;
            case LIBUSB_TRANSFER_CANCELLED:
                LOG_INFO("libusb: Transfer was cancelled");
                UsbCommunicator* comm = (UsbCommunicator*)transfer->user_data;
                (void)SDL_AtomicDecRef(&comm->transfers_in_flight);
                break;
            case LIBUSB_TRANSFER_STALL:
                LOG_WARN("libusb: Endpoint stalled/control request not supported");
                break;
            case LIBUSB_TRANSFER_NO_DEVICE:
                LOG_WARN("libusb: Device was disconnected");
                break;
            case LIBUSB_TRANSFER_OVERFLOW:
                LOG_WARN("libusb: Device sent more data than requested");
                break;
            default:
                LOG_WARN("libusb: Transfer failed with status: %d", transfer->status);
                break;
        }
    }
}

const char* libusb_transfer_flags_to_string(uint8_t flags) {
    static char buffer[256];
    buffer[0] = '\0'; // Ensure the buffer starts as an empty string

    if (flags == 0) {
        return "No flags set";
    }

    if (flags & LIBUSB_TRANSFER_FREE_BUFFER) {
        SDL_strlcat(buffer, "LIBUSB_TRANSFER_FREE_BUFFER ", 256);
    }
    if (flags & LIBUSB_TRANSFER_FREE_TRANSFER) {
        SDL_strlcat(buffer, "LIBUSB_TRANSFER_FREE_TRANSFER ", 256);
    }

    return buffer;
}

static struct libusb_transfer* alloc_and_prep_transfer_chunk(
    libusb_device_handle* handle, UsbCommunicator* comm, unsigned char* buffer, int buffer_size
) {
    LOG_DEBUG("Allocating transfer with size %d...", buffer_size);
    struct libusb_transfer* transfer = libusb_alloc_transfer(0);
    if (transfer == NULL) {
        LOG_ERROR("libusb: failed to alloc transfer struct");
        return NULL;
    }

    libusb_fill_bulk_transfer(
        transfer,
        handle,
        PUSH2_BULK_EP_OUT,
        buffer,
        buffer_size,
        communicator_transfer_callback,
        comm,
        PUSH2_TRANSFER_TIMEOUT_MS
    );
    // Will explode:
    // transfer->flags |= LIBUSB_TRANSFER_FREE_BUFFER;

    SDL_LockMutex(mut);
    comm->transfers[comm->transfers_count++] = transfer;
    SDL_UnlockMutex(mut);

    SDL_assert(comm->transfers_count <= MAX_TRANSFERS);

    return transfer;
}

static void communicator_start_sending(UsbCommunicator* comm) {
    comm->current_line = 0;

    unsigned char* buffer     = (unsigned char*)SDL_calloc(16, sizeof(unsigned char));
    buffer[0]                 = 0xff;
    buffer[1]                 = 0xcc;
    buffer[2]                 = 0xaa;
    buffer[3]                 = 0x88;
    comm->frame_header_buffer = buffer;
    alloc_and_prep_transfer_chunk(comm->usb_device_handle, comm, buffer, 16);

    for (int i = 0; i < SEND_BUFFER_COUNT; i++) {
        unsigned char* buffer = comm->send_buffers + i * SEND_BUFFER_SIZE;

        struct libusb_transfer* transfer =
            alloc_and_prep_transfer_chunk(comm->usb_device_handle, comm, buffer, SEND_BUFFER_SIZE);

        if (transfer != NULL) {
            communicator_send_next_slice(comm, transfer);
        }
    }
}

static int communicator_poll(void* data) {
    UsbCommunicator*      comm          = (UsbCommunicator*)data;
    static struct timeval timeout_500ms = {0, 500000};

    while (SDL_AtomicGet(&comm->terminate) == SDL_FALSE) {
        int ret = libusb_handle_events_timeout_completed(comm->usb_context, &timeout_500ms, &comm->completed);
        if (ret < 0) {
            LOG_ERROR("libusb: libusb_handle_events_timeout_completed failed");
            return 1;
        }
    }

    LOG_DEBUG("%s", "Poller thread terminated. Bye :-)");

    return 0;
}

static void communicator_init(UsbCommunicator* comm, const pixel_t* data_source) {
    comm->data_source     = data_source;
    comm->transfers_count = 0;
    comm->send_buffers    = SDL_calloc(SEND_BUFFER_SIZE * SEND_BUFFER_COUNT, sizeof(unsigned char));
    comm->completed       = 0;
    comm->completed_mutex = SDL_CreateMutex();

    libusb_context*       context;
    libusb_device_handle* device_handle;
    int                   result;

    result = libusb_init_context(&context, NULL, 0);
    if (result < 0) {
        LOG_ERROR("libusb: Failed to initialize: %s", libusb_error_name(result));
        exit(EXIT_FAILURE);
    }

#if defined(DEBUG)
    libusb_set_debug(context, LIBUSB_LOG_LEVEL_DEBUG);
#else
    libusb_set_debug(context, LIBUSB_LOG_LEVEL_ERROR);
#endif

    device_handle = libusb_open_device_with_vid_pid(context, ABLETON_VENDOR_ID, PUSH2_PRODUCT_ID);
    if (device_handle == NULL) {
        LOG_ERROR("libusb: Failed to open device");
        libusb_exit(context);
        exit(EXIT_FAILURE);
    }

    result = libusb_claim_interface(device_handle, USB_INTERFACE_NUMBER);
    if (result < 0) {
        LOG_ERROR("libusb: Failed to claim interface: %s", libusb_error_name(result));
        libusb_close(device_handle);
        libusb_exit(context);
        exit(EXIT_FAILURE);
    }

    comm->usb_context       = context;
    comm->usb_device_handle = device_handle;
    SDL_AtomicSet(&comm->terminate, SDL_FALSE);

    comm->poll_thread = SDL_CreateThread(communicator_poll, "communicator_poll", (void*)comm);
    communicator_start_sending(comm);
}

static int cancel_transfer(UsbCommunicator* comm, struct libusb_transfer* transfer) {
    SDL_assert(transfer != NULL);

    SDL_LockMutex(mut);
    int result = libusb_cancel_transfer(transfer);
    SDL_UnlockMutex(mut);

    return result;
}

static void free_transfer(struct libusb_transfer** transfer) {
    if (transfer == NULL) {
        return;
    }

    SDL_LockMutex(mut);
    if (*transfer != NULL) {
        libusb_free_transfer(*transfer);
        *transfer = NULL;
    }
    SDL_UnlockMutex(mut);
}

static void communicator_deinit(UsbCommunicator* comm) {
    // Step 1: Cancel transfers.
    for (int i = 0; i < MAX_TRANSFERS; ++i) {
        if (comm->transfers[i] != NULL) {
            LOG_DEBUG("Cancelling transfer %p...", comm->transfers[i]);
            if (cancel_transfer(comm, comm->transfers[i]) < 0) {
                LOG_WARN("Failed to cancel transfer %d", i);
            }
        }
    }

    // Step 2: Wait for them to finish.
    int attempts = 100; // 10 sec max
    while (SDL_AtomicGet(&comm->transfers_in_flight) > 0 && attempts-- > 0) {
        SDL_Delay(100);
    }
    if (attempts <= 0) {
        LOG_WARN("Timeout waiting for transfers to complete");
    }

    // Step 3: Now tell thread to exit.
    SDL_AtomicSet(&comm->terminate, SDL_TRUE);
    SDL_LockMutex(comm->completed_mutex);
    comm->completed = 1;
    SDL_UnlockMutex(comm->completed_mutex);

    int thread_status_code;
    SDL_WaitThread(comm->poll_thread, &thread_status_code);
    SDL_assert(thread_status_code == 0);

    // Step 4: Free memory.
    SDL_free(comm->send_buffers);
    for (int i = 0; i < MAX_TRANSFERS; ++i) {
        if (comm->transfers[i]) {
            free_transfer(&comm->transfers[i]);
        }
    }

    if (comm->frame_header_buffer) {
        SDL_free(comm->frame_header_buffer);
        comm->frame_header_buffer = NULL;
    }

    libusb_release_interface(comm->usb_device_handle, USB_INTERFACE_NUMBER);
    libusb_close(comm->usb_device_handle);
    libusb_exit(comm->usb_context);
    SDL_DestroyMutex(comm->completed_mutex);
}

void push2_init(Push2* push) {
    UsbCommunicator comm    = {0};
    PushDisplay     display = {0};

    mut           = SDL_CreateMutex();
    push->usb     = comm;
    push->display = display;

    push_display_init(&push->display);
    communicator_init(&push->usb, &push->display.data_source[0]);
}

void push2_render_callback(Push2* push, Push2RenderCallback render_callback) {
    push->usb.render_callback = render_callback;
}

void push2_flip(Push2* push) {
    pixel_t*            data         = &push->display.bitmap[0];
    const Uint16        sizex        = SDL_min((Uint16)PUSH_DISPLAY_BITMAP_WIDTH, (Uint16)push->usb.render_width);
    static const Uint16 xor_masks[2] = {0xf3e7, 0xffe7};

    SDL_LockMutex(mut);

    for (int y = 0; y < push->usb.render_height; y++) {
        const Uint8* row_start = push->usb.render_pixels + y * push->usb.render_stride;
        for (int x = 0; x < sizex; x++) {
            const Uint8*  src_pixel = row_start + x * 4;
            const pixel_t pixel     = bgr565_from_rgb(src_pixel);
            *data++                 = pixel ^ xor_masks[x % 2];
        }
        data += PUSH_DISPLAY_BITMAP_WIDTH - sizex;
    }

    SDL_UnlockMutex(mut);

    push_display_flip(&push->display);
}

void push2_deinit(Push2* push) {
    LOG_INFO("Push2: Shutting down...");

    communicator_deinit(&push->usb);

    SDL_DestroyMutex(mut);
}
