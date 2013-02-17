#ifndef MOVE_H__
#define MOVE_H__

#include "position.h"

typedef struct {
    move_t move;
    int score;
} move_stack_t;


//move_stack_t* move_generate(position_t* pos, move_stack_t* move, int type);
move_stack_t* move_generate_legal(position_t* pos, move_stack_t* move);
move_stack_t* move_generate_capture(position_t* pos, move_stack_t* move);
move_stack_t* move_generate_quiet(position_t* pos, move_stack_t* move);
move_stack_t* move_generate_evasion(position_t* pos, move_stack_t* move);
move_stack_t* move_generate_non_evasion(position_t* pos, move_stack_t* move);
//move_stack_t* move_generate_quiet_check(position_t* pos, move_stack_t* move);

#endif

