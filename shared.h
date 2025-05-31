#ifndef CHORDEL_SHARED_H
#define CHORDEL_SHARED_H

#include "SDL.h"
#include "SDL_ttf.h"
#include "cairo.h"
#include "portmidi.h"

#include "vendor/TinySoundFont/tsf.h"
#include "vendor/uthash/uthash.h"

#include "push2.h"
#include "xoroshiro.h"

// Ignore errors in bitset.h.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

#include "vendor/cbitset/bitset.h"

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// TODO: Move defines into main.c that are not used in render_plugin.c, and vice
// versa.
#define NUM_PADS                 64
#define PITCH_CLASS_COUNT        12
#define NOTE_NAMES_COUNT         88
#define AUDIO_FREQ               96000
#define AUDIO_FORMAT             AUDIO_S16
#define AUDIO_CHANNELS           2
#define AUDIO_SAMPLES            4096
#define AUDIO_MAX_VOICES         12
#define MAX_LOADED_FONTS         10
#define TEXT_CACHE_TEXT_BUF_SIZE 128
#define TEMP_BUFFER_SIZE         4096
#define PUSH_IMG_W               960
#define PUSH_IMG_H               160
#define MAX_CHORD_VARIANTS       4

#define PUSH_BUTTON_MASTER   28
#define PUSH_BUTTON_ARP_32_T 43
#define PUSH_BUTTON_ARP_32   42
#define PUSH_BUTTON_ARP_16_T 41

#define SHAPE_ENTRY(name)                            name, SDL_arraysize(name)
#define CHORD_TYPE(symbol_, formula_, type_, shape_) {symbol_, formula_, type_, SHAPE_ENTRY(shape_)}

typedef enum {
    DYAD_POWER_ONE_FIVE,
    DYAD_POWER_ONE_FIVE_REPEATED,

    TRIAD_MAJOR,
    TRIAD_MINOR,
    TRIAD_AUGMENTED,
    TRIAD_DIMINISHED,
    TRIAD_SUS2,
    TRIAD_SUS4,

    TETRAD_MAJOR_SEVENTH,
    TETRAD_MINOR_SEVENTH,
    TETRAD_MINOR_MAJOR_SEVENTH,
    TETRAD_DOMINANT_SEVENTH,
    TETRAD_HALF_DIMINISHED,
    TETRAD_DIMINISHED_SEVENTH,
    TETRAD_SIXTH,
    TETRAD_MINOR_SIXTH,
    TETRAD_ADD_NINE,
    TETRAD_SIX_ADD_NINE,
    TETRAD_SUS4_SEVENTH,

    PENTAD_DOMINANT_NINTH,
    PENTAD_MAJOR_NINTH,
    PENTAD_MINOR_NINTH,
    PENTAD_MINOR_MAJOR_NINTH,
    PENTAD_DOMINANT_FLAT_NINE,
    PENTAD_DOMINANT_SHARP_NINE,

    CHORD_COUNT
} ChordName;

typedef enum {
    MIDI_DEVICE_MODE_INPUT,
    MIDI_DEVICE_MODE_OUTPUT,
} MidiDeviceMode;

typedef enum {
    FONT_NAME_PAD,
    FONT_NAME_CURRENT_CHORD,
    FONT_NAME_CURRENT_CHORD_TYPE,
    FONT_NAME_CURRENT_CHORD_FORMULA,
} FontName;

typedef enum {
    PROGRAM_MODE_DISCOVERY,
    PROGRAM_MODE_TRAINING,
    PROGRAM_MODE_TRAINING_RESULTS,
} ProgramMode;

typedef struct {
    char      text[TEXT_CACHE_TEXT_BUF_SIZE];
    FontName  font_name;
    int       font_size;
    SDL_Color font_color;
} TextCacheKey;

typedef struct {
    TextCacheKey   key;
    SDL_Texture*   texture;
    int            width;
    int            height;
    UT_hash_handle hh;
} TextCacheItem;

typedef struct {
    int width;
    int height;
} TextSize;

typedef struct {
    TTF_Font*   font;
    const char* path;
    int         scaled_size;
} LoadedFont;

typedef struct {
    unsigned char*   data;
    signed long long len;
} FileData;

typedef struct {
    char          symbol[TEXT_CACHE_TEXT_BUF_SIZE];
    char          formula[TEXT_CACHE_TEXT_BUF_SIZE];
    char          type[TEXT_CACHE_TEXT_BUF_SIZE];
    const size_t* shape; // e.g. {4, 7, 11}
    size_t        shape_len;
} ChordType;

typedef SDL_bool (*ChordDetectFunc)(size_t root);

typedef struct {
    ChordName       name;
    ChordDetectFunc detect;
} ChordDetector;

typedef struct {
    Uint8     root_note;
    ChordName chord_name;
    Uint32    start_time_ms;
    SDL_bool  played_correctly;

    // When a chord is played correctly, we need to remember
    // the progress so we can render the progress bar at that time.
    double progress_when_played_correctly;
} TrainingQuestion;

typedef enum {
    TRAINING_MODE_CHORDS,
    TRAINING_MODE_SINGLE_NOTES,
} TrainingMode;

typedef struct {
    TrainingMode      mode;
    TrainingQuestion* questions;
    size_t            len;
    size_t            current_index;
    Uint32            question_started_at_ms;
    Uint32            max_time_per_question_ms;
    SDL_bool          waiting_after_correct;
    Uint32            wait_started_at_ms;

    // Wait after having played the chord correctly and lifting the hands.
    SDL_bool released_after_correct;
    Uint32   released_at_ms;
} Training;

typedef struct {
    SDL_bool     muted;
    SDL_bool     fixed_velocity;
    SDL_bool     show_labels;
    SDL_bool     is_recording;
    SDL_atomic_t should_close;

    SDL_AudioSpec audio_spec;

    Training* training;

    // Arena* arena;
    tsf*        sf;
    FileData    sf_file;
    SDL_Window* window;

    SDL_Renderer*     renderer;
    float             render_scale;
    PortMidiStream*   midi_stream_output;
    PortMidiStream*   midi_stream_input;
    Uint8             pad_view_root;
    SDL_bool          window_open;
    SDL_AudioDeviceID audio_device_id_playback;
    SDL_AudioStream*  recording;
    int               window_width;
    int               window_height;
    bitset_t*         current_notes;
    bitset_t*         current_pads;
    bitset_t*         chord_notes;
    TextCacheItem*    text_cache;
    size_t            loaded_fonts;
    LoadedFont        fonts[MAX_LOADED_FONTS];

    SDL_Thread* thread_read_notes;
    int         thread_read_notes_status;

    SDL_Thread* thread_send_pad_colors;
    int         thread_send_pad_colors_status;

    SDL_mutex* pad_colors_mutex;
    int        pad_colors[NUM_PADS];
    int        pad_colors_prev[NUM_PADS];

    ProgramMode  program_mode;
    TrainingMode training_mode;

    SDL_atomic_t current_chord;
    char         current_chord_pitch_symbol[TEXT_CACHE_TEXT_BUF_SIZE];
    char         current_chord_formula[TEXT_CACHE_TEXT_BUF_SIZE];
    char         current_chord_type[TEXT_CACHE_TEXT_BUF_SIZE];

    Push2            push;
    cairo_surface_t* cairo_surface;
    int              cairo_surface_width;
    int              cairo_surface_height;
    int              cairo_surface_stride;
    cairo_t*         cairo_context;

    Uint32 training_max_time_per_question_ms;

    Uint8 push_pad_color;
    Uint8 push_pad_color_c;
    Uint8 push_pad_color_sharp;
    Uint8 push_pad_color_pressed;

    xoroshiro64_state rng;
} App;

extern const int       note_offsets[NUM_PADS];
extern const char*     note_names[NOTE_NAMES_COUNT];
extern const char*     pitches[];
extern const ChordType chord_types[];

SDL_bool have_current_chord(App* app);

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// Shapes
// FIXME: This is not shared across modules, maybe shove it into shared.c.

// Dyads
static const size_t SHAPE_DYAD_POWER_ONE_FIVE[] = {7};

// Triads
static const size_t SHAPE_TRIAD_MAJOR[]      = {4, 7}; // 1-3-5
static const size_t SHAPE_TRIAD_MINOR[]      = {3, 7}; // 1-♭3-5
static const size_t SHAPE_TRIAD_AUGMENTED[]  = {4, 8}; // 1-3-♯5
static const size_t SHAPE_TRIAD_DIMINISHED[] = {3, 6}; // 1-♭3-♭5
static const size_t SHAPE_TRIAD_SUS2[]       = {2, 7}; // 1-2-5
static const size_t SHAPE_TRIAD_SUS4[]       = {5, 7}; // 1-4-5

// Tetrads
static const size_t SHAPE_TETRAD_MAJOR_SEVENTH[]       = {4, 7, 11};    // 1-3-5-7
static const size_t SHAPE_TETRAD_MINOR_SEVENTH[]       = {3, 7, 10};    // 1-♭3-5-♭7
static const size_t SHAPE_TETRAD_MINOR_MAJOR_SEVENTH[] = {3, 7, 11};    // 1-♭3-5-7
static const size_t SHAPE_TETRAD_DOMINANT_SEVENTH[]    = {4, 7, 10};    // 1-3-5-♭7
static const size_t SHAPE_TETRAD_HALF_DIMINISHED[]     = {3, 6, 10};    // 1-♭3-♭5-♭7
static const size_t SHAPE_TETRAD_DIMINISHED_SEVENTH[]  = {3, 6, 9};     // 1-♭3-♭5-♭♭7
static const size_t SHAPE_TETRAD_SIXTH[]               = {4, 7, 9};     // 1-3-5-6
static const size_t SHAPE_TETRAD_MINOR_SIXTH[]         = {3, 7, 9};     // 1-♭3-5-6
static const size_t SHAPE_TETRAD_ADD_NINE[]            = {4, 7, 14};    // 1-3-5-9
static const size_t SHAPE_TETRAD_SIX_ADD_NINE[]        = {4, 7, 9, 14}; // 1-3-5-6-9
static const size_t SHAPE_TETRAD_SUS4_SEVENTH[]        = {5, 7, 10};    // 1-4-5-♭7

// Pentads
static const size_t SHAPE_PENTAD_DOMINANT_NINTH[]      = {4, 7, 10, 14}; // 1-3-5-♭7-9
static const size_t SHAPE_PENTAD_MAJOR_NINTH[]         = {4, 7, 11, 14}; // 1-3-5-7-9
static const size_t SHAPE_PENTAD_MINOR_NINTH[]         = {3, 7, 10, 14}; // 1-♭3-5-♭7-9
static const size_t SHAPE_PENTAD_MINOR_MAJOR_NINTH[]   = {3, 7, 11, 14}; // 1-♭3-5-7-9
static const size_t SHAPE_PENTAD_DOMINANT_FLAT_NINE[]  = {4, 7, 10, 13}; // 1-3-5-♭7-♭9
static const size_t SHAPE_PENTAD_DOMINANT_SHARP_NINE[] = {4, 7, 10, 15}; // 1-3-5-♭7-♯9

#endif
