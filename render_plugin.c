#include "render_plugin.h"
#include "push2.h"

static void render_menu(App* app);
static void render_menu_item(App* app, const char* text, const SDL_bool active, double xoffset, double y);
static void render_current_chord(App* app);
static void render_training_question(App* app);
static void render_training_results(App* app);

static void format_chord_variants(const TrainingQuestion* q, char* buffer, size_t buffer_len);

static void
cairo_show_text_h_align_l(cairo_t* cr, const char* text, int x, int y, const char* font_name, size_t font_size);
static void
cairo_show_text_h_align_r(cairo_t* cr, const char* text, int x, int y, const char* font_name, size_t font_size);
static void
cairo_show_text_h_center(cairo_t* cr, const char* text, int x, int y, const char* font_name, size_t font_size);
static cairo_text_extents_t cairo_measure_text(cairo_t* cr, const char* text, const char* font_name, size_t font_size);

static void cairo_circle(cairo_t* cr, int x, int y, int radius, SDL_bool filled, double line_width);
static void
cairo_circle_partial(cairo_t* cr, int x, int y, int radius, SDL_bool filled, double line_width, double cool01);

void render_callback(App* app, unsigned char** pixels, int* width, int* height, int* stride) {
    // cairo_set_antialias(app->cairo_context, CAIRO_ANTIALIAS_BEST);

    // Always clear background.
    cairo_set_source_rgb(app->cairo_context, 0.0, 0.0, 0.0);
    cairo_paint(app->cairo_context);

    render_menu(app);

    switch (app->program_mode) {
        case PROGRAM_MODE_DISCOVERY: {
            // Render chord if we detected that there is one.
            if (have_current_chord(app) == SDL_TRUE) {
                render_current_chord(app);
            }
            break;
        }

        case PROGRAM_MODE_TRAINING:
            render_training_question(app);
            break;

        case PROGRAM_MODE_TRAINING_RESULTS:
            render_training_results(app);
            break;
    }

    // Draw Cairo surface onto the push display.
    *pixels = cairo_image_surface_get_data(app->cairo_surface);
    *width  = cairo_image_surface_get_width(app->cairo_surface);
    *height = cairo_image_surface_get_height(app->cairo_surface);
    *stride = cairo_image_surface_get_stride(app->cairo_surface);
}

static void render_menu(App* app) {
    // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
    // Lines

    // cairo_set_source_rgb(app->cairo_context, 1.0, 1.0, 1.0);
    // int x = 5;
    // int y = 40;
    // cairo_set_line_width(app->cairo_context, 2.0);
    // cairo_move_to(app->cairo_context, x, y);
    // cairo_line_to(app->cairo_context, PUSH_IMG_W - x, y);
    // cairo_stroke(app->cairo_context);

    // y = 120;
    // cairo_set_line_width(app->cairo_context, 2.0);
    // cairo_move_to(app->cairo_context, x, y);
    // cairo_line_to(app->cairo_context, PUSH_IMG_W - x, y);
    // cairo_stroke(app->cairo_context);

    // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
    // Menu Items

    render_menu_item(app, "DISCOVERY", app->program_mode == PROGRAM_MODE_DISCOVERY, 0, 0);
    render_menu_item(app, "TRAINING", app->program_mode == PROGRAM_MODE_TRAINING, 115, 0);

    if (app->program_mode == PROGRAM_MODE_TRAINING) {
        render_menu_item(app, "NOTES", app->training_mode == TRAINING_MODE_SINGLE_NOTES, 0, 120);
        render_menu_item(app, "CHORDS", app->training_mode == TRAINING_MODE_CHORDS, 115, 120);
    }
}

static void render_menu_item(App* app, const char* text, const SDL_bool active, double xoffset, double y) {
    double rect_x = 5 + xoffset;
    double rect_w = 115;
    double rect_h = 40;

    if (active == 1) {
        cairo_set_source_rgb(app->cairo_context, 1.0, 1.0, 1.0);
        cairo_rectangle(app->cairo_context, rect_x, y, rect_w, rect_h);
        cairo_fill(app->cairo_context);
        cairo_set_source_rgb(app->cairo_context, 0.0, 0.0, 0.0);
    } else {
        cairo_set_source_rgb(app->cairo_context, 1.0, 1.0, 1.0);
        // cairo_rectangle(app->cairo_context, rect_x + 1, y + 1, rect_w - 1, rect_h - 2);
        // cairo_set_line_width(app->cairo_context, 2.0);
        // cairo_stroke(app->cairo_context);
    }

    const char* font_name = "Inter";
    size_t      font_size = 16;

    cairo_select_font_face(app->cairo_context, font_name, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(app->cairo_context, (int)font_size);
    cairo_text_extents_t extents;
    cairo_text_extents(app->cairo_context, text, &extents);
    cairo_move_to(app->cairo_context, rect_x + (rect_w * 0.5 - extents.width * 0.5), y + extents.height * 2.25);
    cairo_show_text(app->cairo_context, text);
}

static void render_current_chord(App* app) {
    cairo_set_source_rgb(app->cairo_context, 1.0, 1.0, 1.0);

    // Type is on the left edge.
    cairo_show_text_h_align_r(app->cairo_context, app->current_chord_type, (960 / 2) - 200, 100, "Inter Display", 24);

    // Pitch Symbol is in the middle.
    cairo_show_text_h_center(
        app->cairo_context, app->current_chord_pitch_symbol, 960 / 2, 100, "Inter Display ExtraBold", 80
    );

    // Formula is on the right edge.
    cairo_show_text_h_align_l(
        app->cairo_context, app->current_chord_formula, (960 / 2) + 200, 100, "JetBrainsMonoNL Nerd Font Mono", 24
    );
}

static void render_training_question(App* app) {
    const char*       chord_font_name = "Inter Display ExtraBold";
    const int         chord_font_size = 80;
    int               top_y           = 80;
    TrainingQuestion* q               = &app->training->questions[app->training->current_index];
    char              chord_buf[32];

    // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
    // Chord + Octave separately

    if (app->training->mode == TRAINING_MODE_CHORDS) {
        const char* root   = pitches[q->root_note % 12];
        const char* symbol = chord_types[q->chord_name].symbol;

        SDL_snprintf(chord_buf, sizeof(chord_buf), "%s%s", root, symbol);

        cairo_set_source_rgb(app->cairo_context, 1.0, 1.0, 1.0);
        cairo_show_text_h_center(
            app->cairo_context, chord_buf, PUSH_IMG_W / 2, top_y, chord_font_name, chord_font_size
        );

        const char* octave_font_name = "Inter Display Thin";
        const int   octave_font_size = chord_font_size;
        int         octave           = (int)(q->root_note / 12) - 1;

        char octave_buf[32];
        SDL_snprintf(octave_buf, sizeof(octave_buf), "%d", octave);

        cairo_text_extents_t chord_text_extents =
            cairo_measure_text(app->cairo_context, chord_buf, chord_font_name, chord_font_size);

        cairo_show_text_h_center(
            app->cairo_context,
            octave_buf,
            (int)(PUSH_IMG_W * 0.5 + chord_text_extents.width * 0.5 + 40.0),
            top_y,
            octave_font_name,
            octave_font_size
        );
    }

    // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
    // Single note without additional octave (its part of the note name already)

    if (app->training->mode == TRAINING_MODE_SINGLE_NOTES) {
        const char* note_name = note_names[SDL_clamp(q->root_note, 21, 108) - 21];

        char buf[32];
        SDL_snprintf(buf, sizeof(buf), "%s", note_name);

        cairo_set_source_rgb(app->cairo_context, 1.0, 1.0, 1.0);
        cairo_show_text_h_center(app->cairo_context, buf, PUSH_IMG_W / 2, top_y, chord_font_name, chord_font_size);
    }

    // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
    // Progress

    double progress;
    if (q->played_correctly) {
        progress = q->progress_when_played_correctly;
    } else {
        Uint32 now     = SDL_GetTicks();
        Uint32 elapsed = now - app->training->question_started_at_ms;
        Uint32 total   = app->training->max_time_per_question_ms;
        progress       = 1.0 - ((double)elapsed / (double)total);
        if (progress < 0.0) {
            progress = 0.0;
        }
    }

    // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
    // Progress color

    double r, g, b = 0.0;
    if (q->played_correctly) {
        r = 0.0;
        g = 1.0;
    } else if (progress > 0.5) {
        double t = (1.0 - progress) * 2.0;
        r        = t;
        g        = 1.0;
    } else {
        double t = (0.5 - progress) * 2.0;
        r        = 1.0;
        g        = 1.0 - t;
    }

    // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
    // Progress indicator for current question

    // cairo_circle_partial(app->cairo_context, PUSH_IMG_W / 2, 50, 100, SDL_FALSE, 20.0, progress);

    double main_progress_w = 300.0;
    double main_progress_h = 10.0;
    double main_progress_x = (PUSH_IMG_W / 2.0) - (main_progress_w / 2.0);
    double main_progress_y = 100;

    cairo_set_source_rgb(app->cairo_context, b, g, r);
    cairo_rectangle(app->cairo_context, main_progress_x, main_progress_y, main_progress_w - 1.0, main_progress_h);
    cairo_set_line_width(app->cairo_context, 2.0);
    cairo_stroke(app->cairo_context);

    cairo_rectangle(app->cairo_context, main_progress_x, main_progress_y, main_progress_w * progress, main_progress_h);
    cairo_fill(app->cairo_context);

    // cairo_set_source_rgb(app->cairo_context, b, g, r);
    // cairo_rectangle(app->cairo_context, 50, 0, (double)PUSH_IMG_W * progress, 40);
    // cairo_fill(app->cairo_context);

    // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
    // Progress indicator for all questions

    /*
    int x      = 240;
    int y      = 140;
    int x_gap  = 10;
    int radius = 5;

    for (Uint32 i = 0; i < (Uint32)app->training->len; ++i) {
        q = &app->training->questions[i];
        x += (int)(radius * 2.0) + x_gap;

        if (i < app->training->current_index) {
            if (q->played_correctly) {
                cairo_set_source_rgb(app->cairo_context, 0.0, 1.0, 0.0);
            } else {
                cairo_set_source_rgb(app->cairo_context, 0.0, 0.0, 1.0);
            }
            cairo_circle(app->cairo_context, x, y, radius + 1, SDL_TRUE, 2.0);
        } else if (i == app->training->current_index) {
            cairo_set_source_rgb(app->cairo_context, b, g, r);
            cairo_circle_partial(app->cairo_context, x, y, radius, SDL_FALSE, 2.0, progress);
        } else {
            cairo_set_source_rgb(app->cairo_context, 1.0, 1.0, 1.0);
            cairo_circle(app->cairo_context, x, y, radius, SDL_FALSE, 2.0);
        }
    }
    */

    {
        double w = main_progress_w / (double)app->training->len;
        double h = main_progress_h * 0.5;
        double x = main_progress_x;
        double y = main_progress_y + 20;

        cairo_set_source_rgb(app->cairo_context, 0.1, 0.1, 0.1);
        cairo_rectangle(app->cairo_context, x, y, main_progress_w, h);
        cairo_fill(app->cairo_context);

        for (Uint32 i = 0; i < (Uint32)app->training->len; ++i) {
            q = &app->training->questions[i];

            if (i < app->training->current_index) {
                if (q->played_correctly) {
                    cairo_set_source_rgb(app->cairo_context, 0.0, 1.0, 0.0);
                    cairo_rectangle(app->cairo_context, x, y, w, h);
                    cairo_fill(app->cairo_context);
                } else {
                    cairo_set_source_rgb(app->cairo_context, 0.0, 0.0, 1.0);
                    cairo_rectangle(app->cairo_context, x, y, w, h);
                    cairo_fill(app->cairo_context);
                }
            } else if (i == app->training->current_index) {
                cairo_set_source_rgb(app->cairo_context, b, g, r);
                cairo_rectangle(app->cairo_context, x, y, w, h);
                cairo_fill(app->cairo_context);
            } else {
                /*
                cairo_set_source_rgb(app->cairo_context, 0.0, 0.0, 0.0);
                cairo_rectangle(
                    app->cairo_context, main_progress_x, main_progress_h, main_progress_w, main_progress_h
                );
                cairo_set_line_width(app->cairo_context, 2.0);
                cairo_stroke(app->cairo_context);
                */
            }

            x += w;
        }
    }

    // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
    // Textual feedback

    // TODO: This is broken. It never shows the correct information.

    /*
    size_t expected_note_count = 1;
    size_t current_note_count  = bitset_count(app->current_notes);

    if (app->training->mode == TRAINING_MODE_CHORDS) {
        expected_note_count += chord_types[q->chord_name].shape_len;
    }

    if (current_note_count == expected_note_count) {
        if (q->played_correctly) {
            cairo_set_source_rgb(app->cairo_context, 0.0, 1.0, 0.0);
            cairo_show_text_h_align_l(app->cairo_context, "CORRECT!", 10, 90, chord_font_name, 40);
        } else {
            if (bitset_count(app->current_notes) > 0) {
                cairo_set_source_rgb(app->cairo_context, 0.0, 0.0, 1.0);
                cairo_show_text_h_align_l(app->cairo_context, "WRONG!", 10, 90, chord_font_name, 40);
            }
        }
    }
    */
}

static void render_training_results(App* app) {
    Uint32 correct = 0;
    Uint32 total   = (Uint32)app->training->len;
    for (Uint32 i = 0; i < total; ++i) {
        if (app->training->questions[i].played_correctly) {
            correct++;
        }
    }

    const char* text           = "You got %u / %u correct!";
    const char* text_font      = "Inter Display ExtraBold";
    const int   text_font_size = 70;

    char buf[128];
    SDL_snprintf(buf, sizeof(buf), text, correct, total);

    cairo_text_extents_t extents = cairo_measure_text(app->cairo_context, text, text_font, text_font_size);

    cairo_set_source_rgb(app->cairo_context, 1.0, 1.0, 1.0);
    cairo_show_text_h_center(
        app->cairo_context,
        buf,
        PUSH_IMG_W / 2,
        (int)((PUSH_IMG_H / 2.0) + (extents.height / 2.0)),
        text_font,
        text_font_size
    );
}

static void cairo_circle(cairo_t* cr, int x, int y, int radius, SDL_bool filled, double line_width) {
    cairo_circle_partial(cr, x, y, radius, filled, line_width, 1.0);
}

static void
cairo_circle_partial(cairo_t* cr, int x, int y, int radius, SDL_bool filled, double line_width, double cool01) {
    SDL_assert(cool01 >= 0 && cool01 <= 1);

    cairo_move_to(cr, x + radius, y);
    cairo_arc(cr, x, y, radius, 0, 2 * 3.141592653589793 * cool01);

    if (filled == SDL_TRUE) {
        cairo_fill(cr);
    } else {
        cairo_set_line_width(cr, line_width);
        cairo_stroke(cr);
    }
}

static cairo_text_extents_t
cairo_measure_text(cairo_t* cr, const char* text, const char* font_name, size_t font_size) {
    cairo_select_font_face(cr, font_name, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, (int)font_size);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, text, &extents);

    return extents;
}

static void
cairo_show_text_h_center(cairo_t* cr, const char* text, int x, int y, const char* font_name, size_t font_size) {
    cairo_text_extents_t extents = cairo_measure_text(cr, text, font_name, font_size);
    cairo_text_extents(cr, text, &extents);
    cairo_move_to(cr, x - (extents.width / 2), y);
    cairo_show_text(cr, text);
}

static void
cairo_show_text_h_align_r(cairo_t* cr, const char* text, int x, int y, const char* font_name, size_t font_size) {
    cairo_text_extents_t extents = cairo_measure_text(cr, text, font_name, font_size);
    cairo_text_extents(cr, text, &extents);
    cairo_move_to(cr, x - extents.width, y);
    cairo_show_text(cr, text);
}

static void
cairo_show_text_h_align_l(cairo_t* cr, const char* text, int x, int y, const char* font_name, size_t font_size) {
    cairo_text_extents_t extents = cairo_measure_text(cr, text, font_name, font_size);
    cairo_text_extents(cr, text, &extents);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);
}
