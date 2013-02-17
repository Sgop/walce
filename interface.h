#ifndef INTERFACE_H__
#define INTERFACE_H__

#include "types.h"
#include "position.h"

typedef struct {
    void (*info_pv)(int score, move_t* pv);
    void (*info_depth)(int depth);
    void (*info_curmove)(move_t move, int num);
    void (*search_done)(position_t* pos, move_t move);
} interface_t;

extern interface_t IF;

#endif
