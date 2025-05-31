// lsusb
// amidi -l
// aseqdump -l

#include "SDL.h"
#include "SDL_ttf.h"
#include "cairo.h"
#include "physfs.h"
#include "physfsrwops.h"
#include "push2.h"

int window_width = 1920;
int window_height = 1080;

cairo_surface_t* surface;
cairo_t* cr;

void render_callback(unsigned char* pixels, int* width, int* height, int* stride) {
    surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 960, 160);
    cr = cairo_create(surface);
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

    pixels = cairo_image_surface_get_data(surface);
    *width = cairo_image_surface_get_width(surface);
    *height = cairo_image_surface_get_height(surface);
    *stride = cairo_image_surface_get_stride(surface);
}

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        SDL_Quit();
        return 1;
    }

#if defined(DEBUG)
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
#else
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
#endif

    const int window_x = SDL_WINDOWPOS_CENTERED_DISPLAY(0);
    const int window_y = SDL_WINDOWPOS_CENTERED_DISPLAY(0);
    const Uint32 window_flags = SDL_WINDOW_BORDERLESS;

    SDL_Window* window =
        SDL_CreateWindow("chordel", window_x, window_y, window_width, window_height, window_flags);
    if (window == NULL) {
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        SDL_Quit();
        return 1;
    }

    Push2 push;
    push2_init(&push);
    push2_render_callback(&push, render_callback);

    SDL_bool quit = SDL_FALSE;
    SDL_Event e;
    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = SDL_TRUE;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);

        push2_flip(&push);
    }

    push2_deinit(&push);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_EVERYTHING);
    SDL_Quit();
}
