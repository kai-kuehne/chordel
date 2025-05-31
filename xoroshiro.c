#include "xoroshiro.h"

static uint32_t rotl32(uint32_t x, int r) {
    return (x << r) | (x >> (32 - r));
}

void xoroshiro64_seed(xoroshiro64_state* rng, uint32_t seed) {
    uint32_t z = seed + 0x9E3779B9;

    z = (z ^ (z >> 16)) * 0x85EBCA6B;
    z = (z ^ (z >> 13)) * 0xC2B2AE35;
    z = z ^ (z >> 16);

    rng->s0 = z;

    z = seed + 0xBB67AE85;
    z = (z ^ (z >> 16)) * 0x85EBCA6B;
    z = (z ^ (z >> 13)) * 0xC2B2AE35;
    z = z ^ (z >> 16);

    rng->s1 = z;
}

uint32_t xoroshiro64_next(xoroshiro64_state* rng) {
    uint32_t s0     = rng->s0;
    uint32_t s1     = rng->s1;
    uint32_t result = s0 + s1;

    s1 ^= s0;

    rng->s0 = rotl32(s0, 26) ^ s1 ^ (s1 << 9);
    rng->s1 = rotl32(s1, 13);

    return result;
}
