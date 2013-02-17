#include "prnd.h"

struct S { uint64_t a, b, c, d; } s;

static uint64_t rotate(uint64_t x, uint64_t k)
{
    return (x << k) | (x >> (64 - k));
}

// Return 64 bit unsigned integer in between [0, 2^64 - 1]
uint64_t prand64()
{
    const uint64_t e = s.a - rotate(s.b,  7);
    s.a = s.b ^ rotate(s.c, 13);
    s.b = s.c + rotate(s.d, 37);
    s.c = s.d + e;
    return s.d = e + s.a;
}

// Init seed and scramble a few rounds
void prnd_init()
{
    s.a = 0xf1ea5eed;
    s.b = s.c = s.d = 0xd4e12c77;
    
    int i;
    for (i = 0; i < 73; i++)
        prand64();
}

