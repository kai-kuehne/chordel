#ifndef CHORDEL_RENDER_PLUGIN_H
#define CHORDEL_RENDER_PLUGIN_H

#include "shared.h"

typedef void (*render_callback_fn)(App* app, unsigned char** pixels, int* width, int* height, int* stride);

#endif
