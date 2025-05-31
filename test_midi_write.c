#include <stdint.h>
#include <stdio.h>

typedef enum {
    META_TYPE_SEQUENCER,
    META_TYPE_ENDOFTRACK,
} MetaType;

typedef struct {
    uint8_t delta;
    // Status:MetaEvent;
    MetaType meta_type;
    uint8_t meta_length;
} Message;

typedef struct {
    uint32_t length;
} TrackChunk;

typedef struct {
    // HeaderFlag flag;
    uint32_t length;
    uint16_t mode;
    uint16_t num_tracks;
} HeaderChunk;

// typedef enum { MThd } HeaderFlag;
// typedef enum TrackFlag { MTrk };

typedef struct {
    HeaderChunk header;
    TrackChunk tracks;
} MidiFile;

void w(FILE* file, uint32_t value, size_t size) {
    for (size_t i = size; i > 0; i--) {
        fputc((value >> ((i - 1) * 8)) & 0xFF, file);
    }
}

int main() {
    FILE* f = fopen("bla.mid", "wb");
    w(f, 'M', 1);
    w(f, 'T', 1);
    w(f, 'h', 1);
    w(f, 'd', 1);
    w(f, 6, 4);
    w(f, 0, 2);
    w(f, 1, 2);
    w(f, 960, 2);

    // First track
    w(f, 'M', 1);
    w(f, 'T', 1);
    w(f, 'r', 1);
    w(f, 'k', 1);

    w(f, 69, 4);
    w(f, 0, 1);
    w(f, 255, 1);
    w(f, 127, 1);
    w(f, 8, 1);

    w(f, 0, 1);
    w(f, 0, 1);
    w(f, 0, 1);
    w(f, 0, 1);
    w(f, 0, 1);
    w(f, 0, 1);
    w(f, 0, 1);
    w(f, 0, 1);

    fclose(f);
}
