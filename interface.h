#ifndef INTERFACE_H__
#define INTERFACE_H__

#include "types.h"
#include "position.h"

typedef struct {
    void (*info_pv)(int score, Move* pv);
    void (*info_depth)(int depth);
    void (*info_done)();
    void (*info_curmove)(Move move, int num);
    void (*search_done)(position_t* pos, Move move);
} interface_t;

extern interface_t IF;

#endif
