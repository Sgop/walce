#ifndef MOVE_H__
#define MOVE_H__

#include "position.h"

typedef struct {
  Move move;
  int score;
} move_stack_t;

enum GenType {
  CAPTURE,
  QUIET,
  QUIET_CHECK,
  EVASION,
  NON_EVASION,
  LEGAL,
};

//template<GenType>
//move_stack_t* generate(const position_t& pos, move_stack_t* moveList);

//move_stack_t* move_generate(position_t* pos, move_stack_t* move, int type);
move_stack_t* move_generate_legal(position_t* pos, move_stack_t* move);
move_stack_t* move_generate_capture(position_t* pos, move_stack_t* move);
move_stack_t* move_generate_quiet(position_t* pos, move_stack_t* move);
move_stack_t* move_generate_evasion(position_t* pos, move_stack_t* move);
move_stack_t* move_generate_non_evasion(position_t* pos, move_stack_t* move);
//move_stack_t* move_generate_quiet_check(position_t* pos, move_stack_t* move);

#endif

