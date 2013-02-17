#ifndef SEARCH_H__
#define SEARCH_H__

#include "position.h"

typedef struct {
    int ply;
    int sValue;
    move_t killers[2];
} search_stack_t;

move_t search_do(position_t* pos);
move_t think(position_t* pos);

uint64_t perft(position_t* pos, int depth);
uint64_t divide(position_t* pos, int depth);

move_t parse_move(position_t* pos, const char* line);


#endif
