#include "shared.h"

const char* note_names[NOTE_NAMES_COUNT] = {
    "A0",  "A#0", "B0",  "C1",  "C#1", "D1",  "D#1", "E1",  "F1",  "F#1", "G1",  "G#1", "A1",  "A#1", "B1",
    "C2",  "C#2", "D2",  "D#2", "E2",  "F2",  "F#2", "G2",  "G#2", "A2",  "A#2", "B2",  "C3",  "C#3", "D3",
    "D#3", "E3",  "F3",  "F#3", "G3",  "G#3", "A3",  "A#3", "B3",  "C4",  "C#4", "D4",  "D#4", "E4",  "F4",
    "F#4", "G4",  "G#4", "A4",  "A#4", "B4",  "C5",  "C#5", "D5",  "D#5", "E5",  "F5",  "F#5", "G5",  "G#5",
    "A5",  "A#5", "B5",  "C6",  "C#6", "D6",  "D#6", "E6",  "F6",  "F#6", "G6",  "G#6", "A6",  "A#6", "B6",
    "C7",  "C#7", "D7",  "D#7", "E7",  "F7",  "F#7", "G7",  "G#7", "A7",  "A#7", "B7",  "C8",
};

const char* pitches[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

const ChordType chord_types[] = {
    [DYAD_POWER_ONE_FIVE]          = CHORD_TYPE("5", "1-5", "Power Chord", SHAPE_DYAD_POWER_ONE_FIVE),
    [DYAD_POWER_ONE_FIVE_REPEATED] = CHORD_TYPE("5", "1-5-1", "Power Chord", SHAPE_DYAD_POWER_ONE_FIVE),

    [TRIAD_MAJOR]      = CHORD_TYPE("maj", "1-3-5", "Major Triad", SHAPE_TRIAD_MAJOR),
    [TRIAD_MINOR]      = CHORD_TYPE("m", "1-b3-5", "Minor Triad", SHAPE_TRIAD_MINOR),
    [TRIAD_AUGMENTED]  = CHORD_TYPE("aug", "1-3-#5", "Augmented Triad", SHAPE_TRIAD_AUGMENTED),
    [TRIAD_DIMINISHED] = CHORD_TYPE("dim", "1-b3-b5", "Diminished Triad", SHAPE_TRIAD_DIMINISHED),
    [TRIAD_SUS2]       = CHORD_TYPE("sus2", "1-2-5", "Suspended Chord", SHAPE_TRIAD_SUS2),
    [TRIAD_SUS4]       = CHORD_TYPE("sus4", "1-4-5", "Suspended Chord", SHAPE_TRIAD_SUS4),

    [TETRAD_MAJOR_SEVENTH] = CHORD_TYPE("maj7", "1-3-5-7", "Major 7th Chord", SHAPE_TETRAD_MAJOR_SEVENTH),
    [TETRAD_MINOR_SEVENTH] = CHORD_TYPE("m7", "1-b3-5-b7", "Minor 7th Chord", SHAPE_TETRAD_MINOR_SEVENTH),
    [TETRAD_MINOR_MAJOR_SEVENTH] =
        CHORD_TYPE("m(maj7)", "1-b3-5-7", "Minor Major 7th Chord", SHAPE_TETRAD_MINOR_MAJOR_SEVENTH),
    [TETRAD_DOMINANT_SEVENTH] = CHORD_TYPE("7", "1-3-5-b7", "Dominant 7th Chord", SHAPE_TETRAD_DOMINANT_SEVENTH),
    [TETRAD_HALF_DIMINISHED] =
        CHORD_TYPE("m7b5", "1-b3-b5-b7", "Half-diminished 7th Chord", SHAPE_TETRAD_HALF_DIMINISHED),
    [TETRAD_DIMINISHED_SEVENTH] =
        CHORD_TYPE("dim7", "1-b3-b5-bb7", "Diminished 7th Chord", SHAPE_TETRAD_DIMINISHED_SEVENTH),
    [TETRAD_SIXTH]        = CHORD_TYPE("6", "1-3-5-6", "6th Chord", SHAPE_TETRAD_SIXTH),
    [TETRAD_MINOR_SIXTH]  = CHORD_TYPE("m6", "1-b3-5-6", "Minor 6th Chord", SHAPE_TETRAD_MINOR_SIXTH),
    [TETRAD_ADD_NINE]     = CHORD_TYPE("add9", "1-3-5-9", "Add Chord", SHAPE_TETRAD_ADD_NINE),
    [TETRAD_SIX_ADD_NINE] = CHORD_TYPE("6/9", "1-3-5-6-9", "6(add9) Chord", SHAPE_TETRAD_SIX_ADD_NINE),
    [TETRAD_SUS4_SEVENTH] = CHORD_TYPE("7sus4", "1-4-5-b7", "Dominant Suspended 4th Chord", SHAPE_TETRAD_SUS4_SEVENTH),

    [PENTAD_DOMINANT_NINTH] = CHORD_TYPE("9", "1-3-5-b7-9", "Dominant 9th Chord", SHAPE_PENTAD_DOMINANT_NINTH),
    [PENTAD_MAJOR_NINTH]    = CHORD_TYPE("maj9", "1-3-5-7-9", "Major 9th Chord", SHAPE_PENTAD_MAJOR_NINTH),
    [PENTAD_MINOR_NINTH]    = CHORD_TYPE("m9", "1-b3-5-♭7-9", "Minor 9th Chord", SHAPE_PENTAD_MINOR_NINTH),
    [PENTAD_MINOR_MAJOR_NINTH] =
        CHORD_TYPE("m(maj9)", "1-b3-5-7-9", "Minor Major 9th Chord", SHAPE_PENTAD_MINOR_MAJOR_NINTH),
    [PENTAD_DOMINANT_FLAT_NINE] =
        CHORD_TYPE("7b9", "1-3-5-b7-b9", "Dominant 7b9 Chord", SHAPE_PENTAD_DOMINANT_FLAT_NINE),
    [PENTAD_DOMINANT_SHARP_NINE] =
        CHORD_TYPE("7#9", "1-3-5-b7-#9", "Dominant 7#9 Chord", SHAPE_PENTAD_DOMINANT_SHARP_NINE),
};

SDL_bool have_current_chord(App* app) {
    return SDL_AtomicGet(&app->current_chord) > 0 ? SDL_TRUE : SDL_FALSE;
}
