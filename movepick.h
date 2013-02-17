#ifndef MOVE_PICK_H__
#define MOVE_PICK_H__

#include "move.h"
#include "position.h"
#include "history.h"
#include "search.h"

enum {
    MP_MSEARCH, S1_CAPTURE, S1_KILLER, S1_QUIET, S1_BAD_CAPTURE,
    MP_ESEARCH, S2_EVASION,
    MP_QSEARCH, S3_CAPTURE,
    MP_STOP,
};


typedef struct {
    position_t* pos;
    int mode;
    move_t pvMove;
    move_stack_t killers[2];
    history_t* hist;
    move_stack_t move[MAX_MOVES];
    move_stack_t* Cur;
    move_stack_t* last;
    move_stack_t* lastBad;
} move_pick_t;


void move_pick_init(
        move_pick_t* picker, position_t* pos, search_stack_t* ss,
        int mode, move_t pvMove, history_t* hist);
move_t move_pick_next(move_pick_t* picker);


#endif
