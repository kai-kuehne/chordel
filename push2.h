#ifndef CHORDEL_PUSH2_H
#define CHORDEL_PUSH2_H

#include "SDL.h"
#include "libusb.h"

#define ABLETON_VENDOR_ID          0x2982
#define LINE_COUNT_PER_SEND_BUFFER 8
#define LINE_SIZE                  2048
#define PUSH2_BULK_EP_OUT          0x01
#define PUSH2_PRODUCT_ID           0x1967
#define PUSH2_TRANSFER_TIMEOUT_MS  1000
#define PUSH_DISPLAY_BITMAP_HEIGHT 160
#define PUSH_DISPLAY_BITMAP_WIDTH  960
#define PUSH_DISPLAY_HEIGHT        160
#define PUSH_DISPLAY_WIDTH         1024
#define SEND_BUFFER_COUNT          3
#define SEND_BUFFER_SIZE           (LINE_SIZE * LINE_COUNT_PER_SEND_BUFFER)
#define USB_INTERFACE_NUMBER       0

// Three per frame + header.
// In total, we only ever have 4 transfer objects. They are re-used.
#define MAX_TRANSFERS (SEND_BUFFER_COUNT + 1)

typedef Uint16 pixel_t;
typedef void (*Push2RenderCallback)(unsigned char** pixels, int* width, int* height, int* stride);

typedef struct {
    libusb_context*         usb_context;
    libusb_device_handle*   usb_device_handle;
    unsigned char*          frame_header_buffer;
    struct libusb_transfer* transfers[MAX_TRANSFERS];
    Uint8                   transfers_count;
    SDL_atomic_t            transfers_in_flight;

    SDL_mutex* completed_mutex;
    int        completed;

    Uint8          current_line;
    unsigned char* send_buffers;
    const pixel_t* data_source;

    SDL_Thread*  poll_thread;
    SDL_atomic_t terminate;

    Push2RenderCallback render_callback;
    unsigned char*      render_pixels;
    int                 render_width;
    int                 render_height;
    int                 render_stride;

} UsbCommunicator;

typedef struct {
    pixel_t data_source[PUSH_DISPLAY_WIDTH * PUSH_DISPLAY_HEIGHT];
    pixel_t bitmap[PUSH_DISPLAY_BITMAP_WIDTH * PUSH_DISPLAY_BITMAP_HEIGHT];
} PushDisplay;

typedef struct {
    UsbCommunicator usb;
    PushDisplay     display;
} Push2;

void push2_init(Push2* push);
void push2_render_callback(Push2* push, Push2RenderCallback render_callback);
void push2_flip(Push2* push);
void push2_deinit(Push2* push);

#endif
