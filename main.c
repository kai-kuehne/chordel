// lsusb
// amidi -l
// aseqdump -l

// TODO: Add new "inversions" mode. Also need to be able to switch between modes.
// TODO: Recording feature #2: Record MIDI!

#include <sys/stat.h>
#include <time.h>

#include "SDL.h"
#include "SDL_loadso.h"
#include "physfs.h"

#include "vendor/physfsrwops/physfsrwops.h"

// Ignore errors in tsf.h.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wnull-pointer-subtraction"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wnull-pointer-subtraction"
#endif
#define TSF_IMPLEMENTATION
#include "vendor/TinySoundFont/tsf.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "shared.h"

#include "log.h"
#include "push2.h"
#include "render_plugin.h"

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// Configuration for main module.

#ifndef DESKTOP_UI
#define DESKTOP_UI 0
#endif

// TODO: Make these configurable using encoders.
#define TRAINING_QUESTIONS_SINGLE_NOTES 20
#define TRAINING_QUESTIONS_CHORDS       10

const char* preferred_audio_devices[] = {"Multi-Output Device"};

const SDL_Color ui_color_pad_bg       = {41, 115, 115, 255};
const SDL_Color ui_color_pad_bg_sharp = {57, 55, 91, 255};
const SDL_Color ui_color_pad_bg_c     = {135, 214, 141, 255};
const SDL_Color ui_color_black        = {0, 0, 0, 255};
const SDL_Color ui_color_white        = {255, 255, 255, 255};

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// Forward declare functions so we can read code top to bottom, starting with
// main().

static void        audio_callback_playback(void* userdata, Uint8* stream, int len);
static void        audio_callback_recording(const void* userdata, Uint8* stream, int len);
static const char* audio_find_device_by_names(const char** names, int names_count);
static void        audio_recording_maybe_draw_indicator();
static void        audio_setup_output(const char* device_name);
static void        audio_recording_start_or_stop();

static void     training_start(size_t num_questions, TrainingMode training_mode);
static void     training_end();
static void     training_check_answer(TrainingQuestion* q);
static void     training_show_hint(const TrainingQuestion* q);
static SDL_bool training_ongoing();

static void ui_init();
static void ui_draw_text(const char* text, FontName font_name, SDL_Color color, int x, int y);
static void ui_draw_current_chord_text();
static void ui_draw_grid();
static void ui_load_font(FontName font_name, const char* path, int unscaled_size);
static void ui_set_render_scale(SDL_Window* window);

static void        bitset_unset(bitset_t* bitset, size_t i);
static SDL_bool    detect_current_chord();
static int         index_to_note(int index);
static SDL_bool    is_chord_playable(uint8_t root, const ChordType* type);
static FileData    load_file_data(const char* file_name);
static void        load_render_plugin(const char* path);
static void        log_and_panic(const char* title);
static void        log_and_panic_message(const char* title, const char* message);
static bool        match_chord_shape(const bitset_t* notes, size_t root, const size_t* shape, size_t shape_len);
static void        maybe_reload_render_plugin();
static SDL_bool    note_is_sharp(int note);
static void        note_off(int note);
static void        note_on(int note, float velocity01);
static const char* note_to_name(int note);
static int         note_to_index(int note);
static PmDeviceID  push_midi_device_id(MidiDeviceMode mode);
static void        push_set_pad_color(int pad, int color);
static void        print_physfs_error_and_exit(void);
static void        print_portmidi_error(PmError error);
static int         rand_between(int min, int max);
static int         read_notes(void* data);
static void        refresh_pad_colors();
static void        render_callback_forwarder(unsigned char** pixels, int* width, int* height, int* stride);
static int         send_pad_colors();
static void        shutdown();
static void        write_audiostream_to_file(const char* filename);
static void        write_wav_header(SDL_RWops* file, const SDL_AudioSpec* spec, Uint32 data_length);

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// Global application state D:

// FIXME: Put these in App?
static time_t             render_plugin_last_mtime   = 0;
static const char*        render_plugin_path         = "render_plugin.so";
static void*              render_plugin_handle       = NULL;
static render_callback_fn render_plugin_callback_ptr = NULL;

static App app = {0};

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// Application

int main(int argc, char** argv) {
#if DEBUG
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
#else
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
#endif

    if (argc < 2) {
        SDL_Log("Usage: %s <path-to-soundfont.sf2>\n", argv[0]);
        return 1;
    }
    const char *soundfont_path = argv[1];

    cairo_surface_t* cs = cairo_image_surface_create(CAIRO_FORMAT_RGB24, PUSH_IMG_W, PUSH_IMG_H);

    app = (App){
        .audio_device_id_playback          = 0,
        .cairo_context                     = cairo_create(cs),
        .cairo_surface                     = cs,
        .chord_notes                       = bitset_create_with_capacity(NOTE_NAMES_COUNT),
        .current_chord                     = {0},
        .current_notes                     = bitset_create_with_capacity(NOTE_NAMES_COUNT),
        .current_pads                      = bitset_create_with_capacity(NOTE_NAMES_COUNT),
        .fixed_velocity                    = SDL_TRUE,
        .is_recording                      = SDL_FALSE,
        .loaded_fonts                      = 0,
        .muted                             = SDL_FALSE,
        .pad_colors_mutex                  = SDL_CreateMutex(),
        .pad_view_root                     = 36, // Start with bottom left pad as C2.
        .program_mode                      = PROGRAM_MODE_DISCOVERY,
        .training_mode                     = TRAINING_MODE_SINGLE_NOTES,
        .push_pad_color                    = 54,
        .push_pad_color_c                  = 13,
        .push_pad_color_pressed            = 1,
        .push_pad_color_sharp              = 0,
        .render_scale                      = 1.0,
        .show_labels                       = SDL_FALSE,
        .text_cache                        = NULL,
        .training                          = NULL,
        .training_max_time_per_question_ms = 10000,
        .window_height                     = 1080,
        .window_width                      = 1920,
    };

    xoroshiro64_seed(&app.rng, (uint32_t)time(NULL));

    for (int i = 0; i < NUM_PADS; ++i) {
        app.pad_colors[i]      = -1;
        app.pad_colors_prev[i] = -1;
    }

    SDL_AtomicSet(&app.should_close, SDL_FALSE);

    if (PHYSFS_init(NULL) == 0) {
        print_physfs_error_and_exit();
    }
    if (PHYSFS_mount("assets.zip", NULL, 0) == 0) {
        print_physfs_error_and_exit();
    }

    app.sf_file.data = SDL_LoadFile(soundfont_path, (size_t *)&app.sf_file.len);
    // app.sf_file = load_file_data("assets/sf2/jrhodes3.sf2");
    app.sf      = tsf_load_memory(app.sf_file.data, (int)app.sf_file.len);
    if (app.sf == TSF_NULL) {
        log_and_panic_message("tsf_load_memory", "Failed to load sf2 file");
    }

#if DESKTOP_UI
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        log_and_panic("SDL_Init");
    }

    ui_init();
#else
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0) {
        log_and_panic("SDL_Init");
    }
#endif

    const char* audio_device_name = audio_find_device_by_names(preferred_audio_devices, 1);
    audio_setup_output(audio_device_name);

    if (Pm_Initialize() != pmNoError) {
        log_and_panic_message("Pm_Initialize", "Failed to initialize PortMIDI");
    }

    int pmdi = push_midi_device_id(MIDI_DEVICE_MODE_INPUT);
    if (pmdi < 0) {
        log_and_panic_message("Couldn't find the Push. Is it connected?", "push_midi_device_id < 0");
    }

    if (Pm_OpenInput(&app.midi_stream_input, pmdi, NULL, 512, NULL, NULL) != pmNoError) {
        log_and_panic_message("Pm_OpenInput", "Failed to open MIDI input stream");
    }

    if (Pm_OpenOutput(
            &app.midi_stream_output, push_midi_device_id(MIDI_DEVICE_MODE_OUTPUT), NULL, 512, NULL, NULL, 0
        ) != pmNoError) {
        log_and_panic_message("Pm_OpenOutput", "Failed to open MIDI output stream");
    }

    app.thread_read_notes      = SDL_CreateThread(read_notes, "read_notes", NULL);
    app.thread_send_pad_colors = SDL_CreateThread(send_pad_colors, "send_pad_colors", NULL);

    refresh_pad_colors();

    push2_init(&app.push);
    push2_render_callback(&app.push, render_callback_forwarder);

    SDL_Event event;
    Uint32    render_plugin_last_check     = 0;
    Uint32    render_plugin_check_interval = 500;

    while (1) {
        if (SDL_GetTicks() - render_plugin_last_check >= render_plugin_check_interval) {
            maybe_reload_render_plugin();
            render_plugin_last_check = SDL_GetTicks();
        }

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                goto cleanup;
                // app.window_open = false;
            }
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_m:
                        app.muted = !app.muted;
                        break;
                    case SDLK_v:
                        app.fixed_velocity = !app.fixed_velocity;
                        break;
                    case SDLK_r:
                        audio_recording_start_or_stop();
                        break;
                    case SDLK_PLUS:
                        tsf_reset(app.sf);
                        app.pad_view_root = SDL_clamp(app.pad_view_root + 12, 12, 96);
                        break;
                    case SDLK_MINUS:
                        tsf_reset(app.sf);
                        app.pad_view_root = SDL_clamp(app.pad_view_root - 12, 12, 96);
                        break;
                    case SDLK_HASH:
                        app.show_labels = !app.show_labels;
                        break;
                }
            }
        }

#if DESKTOP_UI
        SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
        SDL_RenderClear(app.renderer);

        ui_draw_grid();
        ui_draw_current_chord_text();
        audio_recording_maybe_draw_indicator();
        SDL_RenderPresent(app.renderer);
#endif

        // FIXME: No need to have this run 500 billion times a second. Maybe throttling it?
        // FIXME: General problem: Data race on app.training: Between
        // `training_ongoing` returning true and ...
        if (training_ongoing()) {
            Uint32 now = SDL_GetTicks();
            // ... this, t might be NULL.
            Training* t = app.training;

            // So here, we gotta test for NULL again. Plus, this check
            // also only makes sense if we have training questions
            // already.

            // The correct solution would be to actually do
            // multithreading correctly and lock the data structure.
            // But, that's effort.
            if (t != NULL && t->len > 0) {
                if (now - t->question_started_at_ms >= t->max_time_per_question_ms) {
                    if (t->current_index + 1 < t->len) {
                        t->current_index++;
                        t->question_started_at_ms = now;
                    } else {
                        app.program_mode = PROGRAM_MODE_TRAINING_RESULTS;
                    }
                }

                if (t->waiting_after_correct && t->released_after_correct && now - t->released_at_ms >= 500) {
                    t->waiting_after_correct  = SDL_FALSE;
                    t->released_after_correct = SDL_FALSE;

                    if (t->current_index + 1 < t->len) {
                        t->current_index++;
                        t->question_started_at_ms = now;
                    } else {
                        app.program_mode = PROGRAM_MODE_TRAINING_RESULTS;
                    }
                }
            }
        }
    }

cleanup:

    shutdown();

    return 0;
}

static void ui_init() {
    if (TTF_Init() < 0) {
        log_and_panic("TTF_Init");
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_ShowCursor(0);

    int    window_x     = SDL_WINDOWPOS_CENTERED_DISPLAY(0);
    int    window_y     = SDL_WINDOWPOS_CENTERED_DISPLAY(0);
    Uint32 window_flags = SDL_WINDOW_ALLOW_HIGHDPI; // | SDL_WINDOW_FULLSCREEN_DESKTOP;
    app.window = SDL_CreateWindow("Chordel", window_x, window_y, app.window_width, app.window_height, window_flags);
    if (app.window == NULL) {
        log_and_panic("SDL_CreateWindow");
    }

    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_PRESENTVSYNC);
    if (app.renderer == NULL) {
        log_and_panic("SDL_CreateRenderer");
    }
    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);

    ui_set_render_scale(app.window);

    ui_load_font(FONT_NAME_PAD, "assets/ttf/inter/interdisplay_medium.ttf", 25);
    ui_load_font(FONT_NAME_CURRENT_CHORD, "assets/ttf/inter/interdisplay_semibold.ttf", 200);
    ui_load_font(FONT_NAME_CURRENT_CHORD_TYPE, "assets/ttf/inter/interdisplay_bold.ttf", 50);
    ui_load_font(FONT_NAME_CURRENT_CHORD_FORMULA, "assets/ttf/JetBrainsMono/jetbrainsmono_bold.ttf", 100);
}

static void load_render_plugin(const char* path) {
    if (render_plugin_handle) {
        SDL_UnloadObject(render_plugin_handle);
    }

    render_plugin_handle = SDL_LoadObject(path);
    if (!render_plugin_handle) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load render plugin: %s", SDL_GetError());
        return;
    }

    render_plugin_callback_ptr = (render_callback_fn)SDL_LoadFunction(render_plugin_handle, "render_callback");
    if (!render_plugin_callback_ptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to find render_callback: %s", SDL_GetError());
        SDL_UnloadObject(render_plugin_handle);
        render_plugin_handle = NULL;
    }
}

static void maybe_reload_render_plugin() {
    struct stat st;
    if (stat(render_plugin_path, &st) == 0) {
        if (st.st_mtime != render_plugin_last_mtime) {
            render_plugin_last_mtime = st.st_mtime;
            SDL_Log("Detected change in %s, reloading...", render_plugin_path);
            load_render_plugin(render_plugin_path);
        }
    }
}

static void audio_recording_maybe_draw_indicator() {
    if (app.is_recording == SDL_FALSE) {
        return;
    }

    SDL_Rect r = (SDL_Rect){
        100,
        100,
        100,
        100,
    };
    float alpha01 = 0.5f * (1.0f + SDL_sinf((float)SDL_GetTicks() * 0.005f));
    SDL_SetRenderDrawColor(app.renderer, 255, 0, 0, (Uint8)(alpha01 * 255));
    SDL_RenderFillRect(app.renderer, &r);
}

static void print_physfs_error_and_exit() {
    const PHYSFS_ErrorCode err           = PHYSFS_getLastErrorCode();
    const char*            error_message = PHYSFS_getErrorByCode(err);
    LOG_ERROR("PHYSFS ERROR: %s (error code: %d)", error_message, err);
    exit(EXIT_FAILURE);
}

static void log_and_panic_message(const char* title, const char* message) {
    LOG_INFO("%s: %s", title, message);
#if DESKTOP_UI
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, NULL);
#endif
    SDL_Quit();
    exit(EXIT_FAILURE);
}

static void log_and_panic(const char* title) {
    log_and_panic_message(title, SDL_GetError());
}

static FileData load_file_data(const char* file_name) {
    if (PHYSFS_exists(file_name) < 1) {
        print_physfs_error_and_exit();
    }

    PHYSFS_File* file_handle = PHYSFS_openRead(file_name);
    if (file_handle == NULL) {
        print_physfs_error_and_exit();
    }

    PHYSFS_sint64 file_size = PHYSFS_fileLength(file_handle);
    LOG_DEBUG("File size for %s: %lld bytes", file_name, file_size);
    if (file_size < 0) {
        print_physfs_error_and_exit();
    }

    unsigned char* file_data;
#if defined(ARENA_ENABLED)
    file_data = NEW(app.arena, unsigned char, file_size);
#else
    file_data = calloc(1, (unsigned long)file_size);
#endif

    if (file_data == NULL) {
        log_and_panic("Malloc failed");
    }

    PHYSFS_sint64 bytes_read = PHYSFS_readBytes(file_handle, (void*)file_data, (PHYSFS_uint64)file_size);
    if (bytes_read < 0 || bytes_read != file_size) {
        print_physfs_error_and_exit();
    }

    if (PHYSFS_close(file_handle) == 0) {
        print_physfs_error_and_exit();
    }

    return (FileData){
        .data = file_data,
        .len  = file_size,
    };
}

static void shutdown() {
    training_end();

    // Turn off pads (uses portmidi).
    for (int pad = 36; pad <= 99; pad++) {
        push_set_pad_color(pad, 0);
    }

    SDL_AtomicSet(&app.should_close, SDL_TRUE);
    SDL_WaitThread(app.thread_read_notes, &app.thread_read_notes_status);
    SDL_assert(app.thread_read_notes_status == 0);
    SDL_WaitThread(app.thread_send_pad_colors, &app.thread_send_pad_colors_status);
    SDL_assert(app.thread_send_pad_colors_status == 0);

    cairo_destroy(app.cairo_context);
    cairo_surface_destroy(app.cairo_surface);

    push2_deinit(&app.push);

    SDL_PauseAudioDevice(app.audio_device_id_playback, SDL_TRUE);

    // tsf
    LOG_INFO("Shutting down tsf...");
    SDL_free(app.sf_file.data);
    tsf_close(app.sf);

    // App
    TextCacheItem *current_item, *tmp;
    HASH_ITER(hh, app.text_cache, current_item, tmp) {
        SDL_DestroyTexture(current_item->texture);
        HASH_DEL(app.text_cache, current_item);
        // NOTE: HASH_DEL *does* some free'ing, but apparently this is still needed.
        // Don't ask.
        SDL_free(current_item);
    }
    bitset_free(app.current_notes);
    bitset_free(app.current_pads);
    bitset_free(app.chord_notes);

    // Portmidi
    LOG_INFO("%s", "Shutting down portmidi...");
    Pm_Close(app.midi_stream_output);
    Pm_Close(app.midi_stream_input);
    Pm_Terminate();

    // Fonts
    LOG_INFO("%s", "Shutting down SDL_TTF...");
    for (size_t i = 0; i < app.loaded_fonts; ++i) {
        LOG_DEBUG("Closing font %s...", app.fonts[i].path);
        TTF_CloseFont(app.fonts[i].font);
    }
    TTF_Quit();

    // physfs (needs to come after tsf and font shutdown)
    LOG_INFO("%s", "Shutting down physfs...");
    PHYSFS_deinit();

    // SDL
    LOG_INFO("%s", "Shutting down SDL...");
    SDL_CloseAudioDevice(app.audio_device_id_playback);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_QuitSubSystem(SDL_INIT_EVERYTHING);
    SDL_Quit();
}

static void ui_load_font(FontName font_name, const char* path, int unscaled_size) {
    int        scaled_size = (int)((float)(unscaled_size)*app.render_scale);
    SDL_RWops* rwops       = PHYSFSRWOPS_openRead(path);
    TTF_Font*  font        = TTF_OpenFontRW(rwops, 1, scaled_size);
    if (font == NULL) {
        log_and_panic("TTF_OpenFont");
    }
    app.fonts[font_name] = (LoadedFont){
        .font        = font,
        .path        = path,
        .scaled_size = scaled_size,
    };
    app.loaded_fonts++;
    SDL_assert(app.loaded_fonts <= MAX_LOADED_FONTS);
}

static const char* audio_find_device_by_names(const char** names, int names_count) {
    int device_count = SDL_GetNumAudioDevices(0);
    for (int i = 0; i < names_count; ++i) {
        const char* target = names[i];
        for (int j = 0; j < device_count; ++j) {
            const char* device_name = SDL_GetAudioDeviceName(j, 0);
            if (device_name && SDL_strcmp(device_name, target) == 0) {
                return device_name;
            }
        }
    }
    return NULL;
}

static void audio_setup_output(const char* device_name) {
    SDL_AudioSpec spec_desired = (SDL_AudioSpec){
        .freq     = AUDIO_FREQ,
        .format   = AUDIO_FORMAT,
        .channels = AUDIO_CHANNELS,
        .samples  = AUDIO_SAMPLES,
        .callback = audio_callback_playback,
    };

    LOG_INFO("Using audio device: %s", device_name);
    app.audio_device_id_playback = SDL_OpenAudioDevice(device_name, 0, &spec_desired, &app.audio_spec, 0);
    if (app.audio_device_id_playback == 0) {
        log_and_panic("SDL_OpenAudioDevice");
    }

    tsf_set_output(app.sf, TSF_STEREO_INTERLEAVED, app.audio_spec.freq, 0);
    if (tsf_set_max_voices(app.sf, AUDIO_MAX_VOICES) == 0) {
        log_and_panic_message("tsf_set_max_voices", "TinySoundFont2: Failed to set max voices");
    }

    SDL_PauseAudioDevice(app.audio_device_id_playback, SDL_FALSE);
}

static int index_to_note(int index) {
    SDL_assert(index >= 0 && index < NUM_PADS);
    int row = index / 8;
    int col = index % 8;
    return app.pad_view_root + col + row * 5;
}

static int note_to_index(int note) {
    int rel = note - app.pad_view_root;
    int row = rel / 5;
    int col = rel % 5;
    if (row < 0 || row >= 8 || col < 0 || col >= 8) {
        return -1;
    }

    return row * 8 + col;
}

static const char* note_to_name(int note) {
    if (note < 0 || note >= 128) {
        return "?";
    }
    return note_names[SDL_clamp(note, 21, 108) - 21];
}

static SDL_bool note_is_sharp(int note) {
    int m12 = note % 12;
    return m12 % 12 == 1 || m12 % 12 == 3 || m12 % 12 == 6 || m12 % 12 == 8 || m12 % 12 == 10;
}

static PmDeviceID push_midi_device_id(MidiDeviceMode mode) {
    for (int i = 0; i < Pm_CountDevices(); i++) {
        int          device_id   = i;
        PmDeviceInfo device_info = *Pm_GetDeviceInfo(device_id);

        if (SDL_strstr(device_info.name, "Ableton Push 2") != NULL) {
            if (mode == MIDI_DEVICE_MODE_INPUT && device_info.input) {
                return device_id;
            }
            if (mode == MIDI_DEVICE_MODE_OUTPUT && device_info.output) {
                return device_id;
            }
        }
    }

    return -1;
}

static void ui_draw_text(const char* text, FontName font_name, SDL_Color color, int x, int y) {
    if (SDL_strlen(text) == 0) {
        return;
    }

    LoadedFont   lf        = app.fonts[font_name];
    TextCacheKey cache_key = (TextCacheKey){
        .font_name  = font_name,
        .font_size  = lf.scaled_size,
        .font_color = color,
    };
    size_t copied_len = SDL_strlcpy(cache_key.text, text, TEXT_CACHE_TEXT_BUF_SIZE);
    if (copied_len >= TEXT_CACHE_TEXT_BUF_SIZE) {
        LOG_WARN("%s", "draw_text: Input text truncated to fit into cache_key.text");
    }

    TextCacheItem l, *p = NULL;
    SDL_memset(&l, 0, sizeof(TextCacheItem));
    l.key = cache_key;
    HASH_FIND(hh, app.text_cache, &l.key, sizeof(TextCacheKey), p);

    if (p == NULL) {
        int w, h;
        if (TTF_SizeUTF8(lf.font, text, &w, &h) < 0) {
            log_and_panic("TTF_SizeUTF8");
        }

        SDL_Surface* surface = TTF_RenderUTF8_Blended(lf.font, text, color);
        if (surface == NULL) {
            log_and_panic("TTF_RenderUTF8_Blended");
        }
        SDL_Texture* texture = SDL_CreateTextureFromSurface(app.renderer, surface);
        if (texture == NULL) {
            log_and_panic("SDL_CreateTextureFromSurface");
        }
        SDL_FreeSurface(surface);

        p = (TextCacheItem*)SDL_malloc(sizeof *p);
        SDL_memset(p, 0, sizeof *p);
        p->key     = cache_key;
        p->texture = texture;
        p->width   = w;
        p->height  = h;
        HASH_ADD(hh, app.text_cache, key, sizeof(TextCacheKey), p);
    }

    int      scaled_width  = (int)((float)p->width / app.render_scale);
    int      scaled_height = (int)((float)p->height / app.render_scale);
    SDL_Rect rect          = (SDL_Rect){
                 .x = x - (scaled_width / 2),
                 .y = y - (scaled_height / 2),
                 .w = scaled_width,
                 .h = scaled_height,
    };

    if (rect.w > 0 && rect.h > 0) {
        if (SDL_RenderCopy(app.renderer, p->texture, NULL, &rect) < 0) {
            log_and_panic("SDL_RenderCopy");
        }
    }
}

static void print_portmidi_error(PmError error) {
    switch (error) {
        case pmNoError:
            // LOG_DEBUG("portmidi: no error");
            break;
        case pmGotData:
            // LOG_DEBUG("portmidi: got data");
            break;
        case pmHostError:
            LOG_ERROR("%s", "portmidi: host error");
            break;
        case pmInvalidDeviceId:
            LOG_ERROR("%s", "portmidi: invalid device id");
            break;
        case pmInsufficientMemory:
            LOG_ERROR("%s", "portmidi: insufficient memory");
            break;
        case pmBufferTooSmall:
            LOG_ERROR("%s", "portmidi: buffer too small");
            break;
        case pmBufferOverflow:
            LOG_ERROR("%s", "portmidi: buffer overflow");
            break;
        case pmBadPtr:
            LOG_ERROR("%s", "portmidi: bad pointer");
            break;
        case pmBadData:
            LOG_ERROR("%s", "portmidi: bad data");
            break;
        case pmInternalError:
            LOG_ERROR("%s", "portmidi: internal error");
            break;
        case pmBufferMaxSize:
            LOG_ERROR("%s", "portmidi: buffer max size");
            break;
        case pmNotImplemented:
            LOG_ERROR("%s", "portmidi: not implemented ");
            break;
        case pmInterfaceNotSupported:
            LOG_ERROR("%s", "portmidi: interface not supported");
            break;
        case pmNameConflict:
            LOG_ERROR("%s", "portmidi: name conflict");
            break;
    }
}

static void push_set_pad_color(int pad, int color) {
    SDL_assert(color >= 0 && color <= 127);

    if (pad >= 36 && pad <= 99) {
        int index = pad - 36;

        SDL_LockMutex(app.pad_colors_mutex);
        app.pad_colors_prev[index] = app.pad_colors[index];
        app.pad_colors[index]      = color;
        SDL_UnlockMutex(app.pad_colors_mutex);
    }
}

static void ui_draw_grid() {
    int gap = 6;
    int h   = 100;
    int w   = 160;

    SDL_Rect r = (SDL_Rect){
        .x = 0,
        .y = 0,
        .w = w,
        .h = h,
    };

    int x = 0;
    int y = app.window_height - 2 * h - 8 * gap;

    SDL_Color text_color;
    SDL_Color color;
    for (int row = 0; row < 8; row++) {
        x = 0;
        y -= h + gap;

        for (int col = 0; col < 8; col++) {
            int index = (int)(row * 8 + col);
            int note  = index_to_note(index);

            // Note text.
            const char* note_text = note_to_name(note);

            // Background rect.
            r.x = x + (app.window_width / 2 - 4 * w + gap);
            r.y = y + (app.window_height / 2 - 4 * h + gap);

            if (note_is_sharp(note)) {
                color      = ui_color_pad_bg_sharp;
                text_color = ui_color_white;
            } else {
                if (note % 12 == 0) {
                    color      = ui_color_pad_bg_c;
                    text_color = ui_color_black;
                } else {
                    color      = ui_color_pad_bg;
                    text_color = ui_color_white;
                }
            }

            SDL_SetRenderDrawColor(app.renderer, color.r, color.g, color.b, color.a);
            SDL_RenderFillRect(app.renderer, &r);

            // Foreground rect.
            if (app.pad_colors[index] == 50) {
                SDL_SetRenderDrawColor(app.renderer, 255, 255, 255, 200);
                text_color = ui_color_black;
            } else {
                SDL_SetRenderDrawColor(app.renderer, 255, 255, 255, 0);
            }
            SDL_RenderFillRect(app.renderer, &r);

            if (app.show_labels) {
                ui_draw_text(note_text, FONT_NAME_PAD, text_color, r.x + w / 2, r.y + h / 2);
            }

            x += w + gap;
        }
    }
}

// NOTE: A separate mutex for this is not necessary if SDL_LockAudioDevice is
// used. audio_callback automatically uses the internal mutex that is used by
// SDL_(Un)LockAudioDevice.
static void audio_callback_playback(void* userdata, Uint8* stream, int len) {
    size_t sample_count = (size_t)(len) / (2 * sizeof(short));

    tsf_render_short(app.sf, (short*)stream, (int)sample_count, 0);
    if (app.recording != NULL) {
        SDL_AudioStreamPut(app.recording, stream, len);
    }
}

static void bitset_unset(bitset_t* bitset, size_t i) {
    bitset_set_to_value(bitset, i, SDL_FALSE);
}

static void ui_draw_current_chord_text() {
    if (have_current_chord(&app) == SDL_TRUE) {
        int x = app.window_width / 2;
        int y = app.window_height / 2;

        ui_draw_text(app.current_chord_pitch_symbol, FONT_NAME_CURRENT_CHORD, ui_color_white, x, y - 100);
        ui_draw_text(app.current_chord_formula, FONT_NAME_CURRENT_CHORD_FORMULA, ui_color_white, x, y + 70);
        ui_draw_text(app.current_chord_type, FONT_NAME_CURRENT_CHORD_TYPE, ui_color_white, x, y + 150);
    }
}

static SDL_bool update_current_chord(size_t root, const ChordName chord_name) {
    // Reset
    SDL_AtomicSet(&app.current_chord, 0);
    SDL_memset(app.current_chord_pitch_symbol, 0, TEXT_CACHE_TEXT_BUF_SIZE);
    SDL_memset(app.current_chord_formula, 0, TEXT_CACHE_TEXT_BUF_SIZE);
    SDL_memset(app.current_chord_type, 0, TEXT_CACHE_TEXT_BUF_SIZE);

    const ChordType chord_type = chord_types[chord_name];

    // Update
    const char* pitch_class_str = pitches[(root + 21) % 12]; // Ignores octave: A, not A3 or A4.
    SDL_snprintf(app.current_chord_pitch_symbol, TEXT_CACHE_TEXT_BUF_SIZE, "%s%s", pitch_class_str, chord_type.symbol);
    SDL_strlcpy(app.current_chord_formula, chord_type.formula, TEXT_CACHE_TEXT_BUF_SIZE);
    SDL_strlcpy(app.current_chord_type, chord_type.type, TEXT_CACHE_TEXT_BUF_SIZE);
    SDL_AtomicSet(&app.current_chord, 1);

    return SDL_TRUE;
}

static void ui_set_render_scale(SDL_Window* window) {
    SDL_GetWindowSize(window, &app.window_width, &app.window_height);
    LOG_INFO("Got: Window size: %d x %d", app.window_width, app.window_height);
    LOG_INFO("Setting logical render size to %d x %d...", app.window_width, app.window_height);
    if (SDL_RenderSetLogicalSize(app.renderer, app.window_width, app.window_height) < 0) {
        log_and_panic("SDL_RenderSetLogicalSize");
    }

    int rw;
    int rh;
    SDL_GetRendererOutputSize(app.renderer, &rw, &rh);
    LOG_INFO("Got: Renderer output size: %d x %d", rw, rh);
    if (rw != app.window_width || rh != app.window_height) {
        float width_scale  = (float)rw / (float)app.window_width;
        float height_scale = (float)rh / (float)app.window_height;

        if (width_scale != height_scale) {
            LOG_WARN("Render width scale != render height scale");
        }

        LOG_INFO("Setting horizontal render scale to %f...", (double)width_scale);
        LOG_INFO("Setting vertical render scale %f...", (double)width_scale);
        SDL_RenderSetScale(app.renderer, width_scale, height_scale);
        app.render_scale = width_scale;
    }
    LOG_INFO("Render scale: %.1f", (double)app.render_scale);
}

static void render_callback_forwarder(unsigned char** pixels, int* width, int* height, int* stride) {
    if (render_plugin_callback_ptr != NULL) {
        render_plugin_callback_ptr(&app, pixels, width, height, stride);
        push2_flip(&app.push);
    }
}

static void note_on(int note, float velocity01) {
    SDL_LockAudioDevice(app.audio_device_id_playback);
    tsf_note_on(app.sf, 0, note, velocity01);
    SDL_UnlockAudioDevice(app.audio_device_id_playback);
}

static void note_off(int note) {
    SDL_LockAudioDevice(app.audio_device_id_playback);
    tsf_note_off(app.sf, 0, note);
    SDL_UnlockAudioDevice(app.audio_device_id_playback);
}

static SDL_bool is_chord_playable(uint8_t root, const ChordType* type) {
    if (!type->shape || type->shape_len == 0) {
        return false;
    }

    // Compute visible MIDI note range.
    int min_note = index_to_note(0);
    int max_note = index_to_note(NUM_PADS - 1);

    // Root must be visible.
    if (root < min_note || root > max_note) {
        return false;
    }

    // All offsets must stay within the visible range.
    for (size_t i = 0; i < type->shape_len; ++i) {
        int note = (int)(root + type->shape[i]);
        if (note < min_note || note > max_note) {
            return false;
        }
    }

    return true;
}

static int read_notes(void* data) {
    while (SDL_AtomicGet(&app.should_close) == SDL_FALSE) {
        if (Pm_Poll(app.midi_stream_input) == pmGotData) {
            PmEvent event;
            if (Pm_Read(app.midi_stream_input, &event, 1) > 0) {
                int status   = Pm_MessageStatus(event.message);
                int note     = Pm_MessageData1(event.message);
                int velocity = Pm_MessageData2(event.message);
                LOG_DEBUG("Received MIDI message - Status: %x, Note: %d, Velocity: %d\n", status, note, velocity);

                // Non-grid buttons on the push send status b0, not 0x80 or x90.
                if (status == 0xb0) {

                    // Switch mode to DISCOVERY.
                    if (note == 102 && velocity > 0) {
                        app.program_mode = PROGRAM_MODE_DISCOVERY;
                    }

                    if (note == 103 && velocity > 0) {
                        // FIXME: Remember what the last training mode
                        // was and re-use that here instead of always
                        // doing single notes training.
                        training_start(TRAINING_QUESTIONS_SINGLE_NOTES, TRAINING_MODE_SINGLE_NOTES);
                    }

                    if (app.program_mode == PROGRAM_MODE_TRAINING) {
                        if (note == 20 && velocity > 0) {
                            training_start(TRAINING_QUESTIONS_SINGLE_NOTES, TRAINING_MODE_SINGLE_NOTES);
                        }

                        if (note == 21 && velocity > 0) {
                            training_start(TRAINING_QUESTIONS_CHORDS, TRAINING_MODE_CHORDS);
                        }
                    }

                    // Change pad colors.
                    if (note >= PUSH_BUTTON_ARP_16_T && note <= PUSH_BUTTON_ARP_32_T && velocity > 0) {
                        switch (note) {
                            case PUSH_BUTTON_ARP_32_T:
                                // Change colors for all C's.
                                app.push_pad_color_c = (app.push_pad_color_c + 1) % 128;
                                break;
                            case PUSH_BUTTON_ARP_32:
                                // Change colors for all notes in scale.
                                app.push_pad_color = (app.push_pad_color + 1) % 128;
                                break;
                            case PUSH_BUTTON_ARP_16_T:
                                // Change colors for all sharp notes.
                                // Since the grid is in C major, this is the same as notes that
                                // are "not in the scale".
                                app.push_pad_color_sharp = (app.push_pad_color_sharp + 1) % 128;
                                break;
                        }
                        LOG_INFO(
                            "New pad note colors: %d - C's: %d - #'s: %d",
                            app.push_pad_color,
                            app.push_pad_color_c,
                            app.push_pad_color_sharp
                        );
                        refresh_pad_colors();
                    }

                    if (note == PUSH_BUTTON_MASTER && training_ongoing()) {
                        if (velocity > 0) {
                            TrainingQuestion* q = &app.training->questions[app.training->current_index];
                            training_show_hint(q);
                        } else {
                            refresh_pad_colors();
                        }
                    }
                }

                // Ignore midi numbers below 36 and above (36 + NUM_PADS).
                if (note < 36 || note > 36 + NUM_PADS) {
                    continue;
                }

                int index        = note - 36;
                int note_to_play = index_to_note(index);

                switch (status) {

                    // Note on.
                    case 0x90: {

                        if (note >= 36 && note < 36 + NUM_PADS && velocity > 0) {
                            bitset_set(app.current_pads, (size_t)note);
                            bitset_set(app.current_notes, (size_t)note_to_play - 21);
                            LOG_DEBUG("Played pad: %ld", (size_t)note);
                            LOG_DEBUG("Played note: %ld", (size_t)note_to_play - 21);

                            float velocity01 = (float)velocity / 127.0f;
                            if (app.fixed_velocity) {
                                velocity01 = 0.7f;
                            }

                            push_set_pad_color(note, app.push_pad_color_pressed);

                            if (!app.muted) {
                                note_on(note_to_play, velocity01);
                            } else {
                                tsf_reset(app.sf);
                            }

                            if (training_ongoing()) {
                                TrainingQuestion* q = &app.training->questions[app.training->current_index];
                                training_check_answer(q);
                            }
                        }

                        break;
                    }

                    // Note off.
                    case 0x80: {

                        push_set_pad_color(note, app.pad_colors_prev[note - 36]);
                        bitset_unset(app.current_pads, (size_t)note);
                        bitset_unset(app.current_notes, (size_t)note_to_play - 21);

                        if (!app.muted) {
                            note_off(note_to_play);
                        }

                        if (training_ongoing()) {
                            Training* t = app.training;

                            if (t->waiting_after_correct && bitset_count(app.current_pads) == 0) {
                                t->released_after_correct = SDL_TRUE;
                                t->released_at_ms         = SDL_GetTicks();
                            }
                        }

                        break;
                    }
                }
            }

            detect_current_chord();
        }
    }

    return 0;
}

static int send_pad_colors() {
    while (1) {
        if (SDL_AtomicGet(&app.should_close)) {
            for (int index = 0; index < NUM_PADS; ++index) {
                int       pad    = index + 36;
                PmMessage msg    = Pm_Message(144, pad, 0);
                PmError   result = Pm_WriteShort(app.midi_stream_output, 0, msg);
                print_portmidi_error(result);
            }

            return 0;
        }

        for (int index = 0; index < NUM_PADS; ++index) {
            SDL_LockMutex(app.pad_colors_mutex);
            int color      = app.pad_colors[index];
            int color_prev = app.pad_colors_prev[index];
            SDL_UnlockMutex(app.pad_colors_mutex);

            // FIXME: This is true even if colors didn't change, but shouldn't.
            if (color != color_prev) {
                int       pad    = index + 36;
                PmMessage msg    = Pm_Message(144, pad, color);
                PmError   result = Pm_WriteShort(app.midi_stream_output, 0, msg);
                print_portmidi_error(result);
            }
        }

        SDL_Delay(50);
    }

    LOG_INFO("Shutting down pad color thread...");

    return 0;
}

static void write_wav_header(SDL_RWops* file, const SDL_AudioSpec* spec, Uint32 data_length) {
    Uint32 byte_rate       = (Uint32)spec->freq * spec->channels * SDL_AUDIO_BITSIZE(spec->format) / 8;
    Uint16 block_align     = spec->channels * SDL_AUDIO_BITSIZE(spec->format) / 8;
    Uint32 chunk_size      = 36 + data_length;
    Uint32 subchunk1_size  = 16;
    Uint16 audio_format    = 1;
    Uint16 bits_per_sample = SDL_AUDIO_BITSIZE(spec->format);

    SDL_RWwrite(file, "RIFF", 1, 4);
    SDL_RWwrite(file, &chunk_size, 4, 1);
    SDL_RWwrite(file, "WAVE", 1, 4);
    SDL_RWwrite(file, "fmt ", 1, 4);
    SDL_RWwrite(file, &subchunk1_size, 4, 1);
    SDL_RWwrite(file, &audio_format, 2, 1);
    SDL_RWwrite(file, &spec->channels, 2, 1);
    SDL_RWwrite(file, &spec->freq, 4, 1);
    SDL_RWwrite(file, &byte_rate, 4, 1);
    SDL_RWwrite(file, &block_align, 2, 1);
    SDL_RWwrite(file, &bits_per_sample, 2, 1);
    SDL_RWwrite(file, "data", 1, 4);
    SDL_RWwrite(file, &data_length, 4, 1);
}

static void write_audiostream_to_file(const char* filename) {
    LOG_INFO("Stopped recording. Writing stream to %s", filename);
    SDL_AudioStreamFlush(app.recording);

    SDL_RWops* file = SDL_RWFromFile(filename, "wb");
    if (!file) {
        LOG_ERROR("Failed to open output file: %s\n", SDL_GetError());
        SDL_FreeAudioStream(app.recording);
        SDL_Quit();
    }

    Uint32 data_length = 0;
    write_wav_header(file, &app.audio_spec, data_length);

    Uint8  temp_buffer[TEMP_BUFFER_SIZE];
    Uint32 total_data_written = 0;

    while (1) {
        int bytes_fetched = SDL_AudioStreamGet(app.recording, temp_buffer, TEMP_BUFFER_SIZE);
        if (bytes_fetched < 0) {
            LOG_ERROR("Failed to get data from SDL_AudioStream: %s\n", SDL_GetError());
            SDL_RWclose(file);
            SDL_FreeAudioStream(app.recording);
            SDL_Quit();
        }
        if (bytes_fetched == 0) {
            break;
        }

        if (SDL_RWwrite(file, temp_buffer, 1, (size_t)bytes_fetched) != (size_t)bytes_fetched) {
            LOG_ERROR("Failed to write audio data to file: %s\n", SDL_GetError());
            SDL_RWclose(file);
            SDL_FreeAudioStream(app.recording);
            SDL_Quit();
        }

        total_data_written += (Uint32)bytes_fetched;
    }

    SDL_RWseek(file, 0, RW_SEEK_SET);
    write_wav_header(file, &app.audio_spec, total_data_written);
    SDL_RWclose(file);
}

static void audio_recording_start_or_stop() {
    if (app.is_recording == SDL_FALSE) {
        app.recording = SDL_NewAudioStream(
            app.audio_spec.format,
            app.audio_spec.channels,
            app.audio_spec.freq,
            app.audio_spec.format,
            app.audio_spec.channels,
            app.audio_spec.freq
        );

        SDL_assert(app.recording != NULL);
    } else {
        Uint32      timestamp = (Uint32)time(NULL);
        static char filename[64];
        SDL_snprintf(filename, sizeof(filename), "output_%u.wav", timestamp);
        write_audiostream_to_file(filename);
        SDL_FreeAudioStream(app.recording);
        app.recording = NULL;
    }

    app.is_recording = !app.is_recording;
}

static int rand_between(int min, int max) {
    return (int)(xoroshiro64_next(&app.rng) % (Uint32)(max - min + 1)) + min;
}

// NOTE: Call this after changing app.pad_push_color*.
static void refresh_pad_colors() {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int index = (int)(row * 8 + col);
            int note  = index_to_note(index);
            int pad   = index + 36;
            int color;

            if (note_is_sharp(note)) {
                color = app.push_pad_color_sharp;
            } else {
                if (note % 12 == 0) {
                    color = app.push_pad_color_c;
                } else {
                    color = app.push_pad_color;
                }
            }

            push_set_pad_color(pad, color);
        }
    }
}

static SDL_bool detect_current_chord() {
    // FIXME: Check the locking logic.
    SDL_AtomicSet(&app.current_chord, 0);

    for (size_t root = 0; bitset_next_set_bit(app.current_notes, &root); root++) {
        for (ChordName name = 0; name < CHORD_COUNT; ++name) {
            const ChordType* ct = &chord_types[name];
            if (ct->shape && match_chord_shape(app.current_notes, root, ct->shape, ct->shape_len)) {
                return update_current_chord(root, name);
            }
        }
    }

    return SDL_FALSE;
}

static bool match_chord_shape(const bitset_t* notes, size_t root, const size_t* shape, size_t shape_len) {
    bitset_t* candidate = bitset_create_with_capacity(128);
    if (!candidate) {
        return false;
    }

    bitset_clear(candidate);
    bitset_set(candidate, root);

    for (size_t i = 0; i < shape_len; ++i) {
        bitset_set(candidate, root + shape[i]);
    }

    bool match = (bitset_symmetric_difference_count(notes, candidate) == 0);

    bitset_free(candidate);
    return match;
}

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// TRAINING

static void training_start(size_t num_questions, TrainingMode training_mode) {
    training_end();

    app.program_mode  = PROGRAM_MODE_TRAINING;
    app.training_mode = training_mode;

    LOG_INFO("Starting new training session with %zu questions", num_questions);

    if (app.training == NULL) {
        app.training = SDL_calloc(1, sizeof(Training));
        if (app.training == NULL) {
            LOG_ERROR("Failed to allocate training session");
            return;
        }
    }

    Training* training                 = app.training;
    training->mode                     = app.training_mode;
    training->max_time_per_question_ms = app.training_max_time_per_question_ms;
    training->question_started_at_ms   = SDL_GetTicks();
    training->questions                = SDL_calloc(num_questions, sizeof(TrainingQuestion));
    training->len                      = num_questions;
    training->current_index            = 0;

    if (training->questions == NULL) {
        LOG_ERROR("Failed to allocate training questions");
        training->len = 0;
        return;
    }

    const Sint32 min_note = index_to_note(0);
    const Sint32 max_note = index_to_note(NUM_PADS - 1);
    const size_t range    = (size_t)(max_note - min_note + 1);

    for (size_t i = 0; i < num_questions; ++i) {
        ChordName chord;
        Uint8     root = 0;

        switch (app.training_mode) {
            case TRAINING_MODE_CHORDS: {
                const ChordType* type = NULL;

                while (SDL_TRUE) {
                    chord = (ChordName)rand_between(0, CHORD_COUNT - 1);
                    type  = &chord_types[chord];

                    Uint32 note = (Uint32)(min_note + rand_between(0, (int)range - 1));
                    if (note <= 127) {
                        root = (Uint8)note;
                        if (is_chord_playable(root, type)) {
                            break;
                        }
                    }
                }

                break;
            }

            case TRAINING_MODE_SINGLE_NOTES: {
                root = (Uint8)(min_note + rand_between(0, (int)range - 1));
                // FIXME: Maybe: Don't care about chord in single note training mode.
                chord = 0;

                break;
            }
        }

        training->questions[i].root_note                      = root;
        training->questions[i].chord_name                     = chord;
        training->questions[i].played_correctly               = SDL_FALSE;
        training->questions[i].progress_when_played_correctly = 0.0;
        training->questions[i].start_time_ms                  = SDL_GetTicks();
    }
}

static void training_end() {
    if (training_ongoing() == SDL_FALSE) {
        return;
    }

    SDL_free(app.training->questions);
    app.training->questions     = NULL;
    app.training->len           = 0;
    app.training->current_index = 0;

    SDL_free(app.training);
    app.training = NULL;
}

static SDL_bool training_ongoing() {
    return app.program_mode == PROGRAM_MODE_TRAINING && app.training != NULL;
}

static void training_check_answer(TrainingQuestion* q) {
    bitset_t* expected = bitset_create_with_capacity(NOTE_NAMES_COUNT);

    const ChordType* type = &chord_types[q->chord_name];

    bitset_set(expected, q->root_note - 21);

    // NOTE: The above covers both chords and single note training
    // modes. For chord training, we just have to add the other notes
    // of the chords in addition to the root note.
    if (app.training->mode == TRAINING_MODE_CHORDS) {
        for (size_t i = 0; i < type->shape_len; ++i) {
            bitset_set(expected, q->root_note + type->shape[i] - 21);
        }
    }

    LOG_DEBUG("Checking training answer...");
#if defined(DEBUG)
    bitset_print(expected);
    printf("\n");
    bitset_print(app.current_notes);
    printf("\n");
#endif

    size_t diff_count = bitset_symmetric_difference_count(expected, app.current_notes);
    bitset_free(expected);

    if (diff_count == 0) {
        q->played_correctly      = SDL_TRUE;
        Training* t              = app.training;
        t->waiting_after_correct = SDL_TRUE;
        t->wait_started_at_ms    = SDL_GetTicks();

        Uint32 now     = SDL_GetTicks();
        Uint32 elapsed = now - app.training->question_started_at_ms;
        Uint32 total   = app.training->max_time_per_question_ms;

        q->progress_when_played_correctly = 1.0 - (double)elapsed / (double)total;
        if (q->progress_when_played_correctly < 0.0) {
            q->progress_when_played_correctly = 0.0;
        }
    }
}

static void training_show_hint(const TrainingQuestion* q) {
    switch (app.training->mode) {
        case TRAINING_MODE_CHORDS: {
            const ChordType* type       = &chord_types[q->chord_name];
            int              root_index = note_to_index(q->root_note);
            if (root_index == -1) {
                return;
            }

            int root_row = root_index / 8;

            push_set_pad_color(root_index + 36, 77);

            for (size_t i = 0; i < type->shape_len; ++i) {
                int note = q->root_note + (int)type->shape[i];

                int match_index = -1;
                for (int j = 0; j < NUM_PADS; ++j) {
                    if (index_to_note(j) != note) {
                        continue;
                    }

                    int row = j / 8;
                    if (row == root_row) {
                        match_index = j;
                        break;
                    }
                }

                if (match_index == -1) {
                    for (int j = 0; j < NUM_PADS; ++j) {
                        if (index_to_note(j) == note) {
                            match_index = j;
                            break;
                        }
                    }
                }

                if (match_index != -1) {
                    push_set_pad_color(match_index + 36, 77);
                }
            }
        }

        case TRAINING_MODE_SINGLE_NOTES: {
            // TODO: Bug: For some reason, showing a hint for F5 is not working.
            // The grid contains (without octave shifting) C2 -> F#5.
            int root_index = note_to_index(q->root_note);
            push_set_pad_color(root_index + 36, 77);
            break;
        }
    }
}
