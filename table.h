#ifndef TRANSPOSITION_H__
#define TRANSPOSITION_H__

#include <stdlib.h>
#include "types.h"


typedef uint32_t pkey32_t;

typedef struct {
    pkey32_t key;    // 32 bit
    move_t move;     // 16 bit
    uint8_t bound;   //  8 bit
    uint8_t age;     //  8 bit
    int16_t value;   // 16 bit
    int16_t depth;   // 16 bit
    int16_t sValue;  // 16 bit
} tentry_t;

//extern ttable_t* TT;

void tables_init();
void tables_exit();


void TT_store(
        pkey_t key, int16_t value, uint8_t bound, int16_t depth, move_t move,
        int16_t sValue);
tentry_t* TT_probe(pkey_t key);
void TT_advance();
void TT_touch(tentry_t* tte);
void TT_clear();


#endif

