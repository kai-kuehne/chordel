#ifndef XOROSHIRO_H
#define XOROSHIRO_H

#include <stdint.h>

typedef struct {
    uint32_t s0;
    uint32_t s1;
} xoroshiro64_state;

void     xoroshiro64_seed(xoroshiro64_state* rng, uint32_t seed);
uint32_t xoroshiro64_next(xoroshiro64_state* rng);

#endif
