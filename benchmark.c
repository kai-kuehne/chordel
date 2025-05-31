#include "SDL.h"
#include "cairo.h"
#include "push2.h"

int window_width  = 1920;
int window_height = 1080;

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_TIMER) < 0) {
        SDL_Quit();
        return 1;
    }

    Push2 push;
    push2_init(&push);

    cairo_surface_t* surface;
    cairo_t*         cr;
    surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 960, 160);
    cr      = cairo_create(surface);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);
    cairo_set_line_width(cr, 40.96);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_move_to(cr, 100, 110);
    cairo_rel_line_to(cr, 51.2, -51.2);
    cairo_rel_line_to(cr, 51.2, 51.2);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
    cairo_stroke(cr);
    cairo_set_line_width(cr, 40.96);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_move_to(cr, 960 - 200, 110);
    cairo_rel_line_to(cr, 51.2, -51.2);
    cairo_rel_line_to(cr, 51.2, 51.2);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL);
    cairo_stroke(cr);
    cairo_set_line_width(cr, 40.96);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_move_to(cr, 960 / 2 - 50, 110);
    cairo_rel_line_to(cr, 51.2, -51.2);
    cairo_rel_line_to(cr, 51.2, 51.2);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_stroke(cr);

    unsigned char* s_pixels = cairo_image_surface_get_data(surface);
    int            s_width  = cairo_image_surface_get_width(surface);
    int            s_height = cairo_image_surface_get_height(surface);
    int            s_stride = cairo_image_surface_get_stride(surface);

    const int iterations = 100000;
    Uint64    start      = SDL_GetPerformanceCounter();
    for (int i = 0; i < iterations; i++) {
        push2_flip(&push, s_pixels, s_width, s_height, s_stride);
    }
    Uint64 end         = SDL_GetPerformanceCounter();
    Uint64 elapsed     = end - start;
    double seconds     = (double)elapsed / SDL_GetPerformanceFrequency();
    double avg_time_ms = (seconds * 1000) / iterations;
    printf("Total time for %d iterations: %.3f s\n", iterations, seconds);
    printf("Average time per iteration: %.3f ms\n", avg_time_ms);

    push2_deinit(&push);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    SDL_QuitSubSystem(SDL_INIT_TIMER);
    SDL_Quit();
}
