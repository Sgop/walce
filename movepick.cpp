#include "movepick.h"


void move_pick_init(
        move_pick_t* picker, position_t* pos, search_stack_t* ss,
        int mode, Move pvMove, history_t* hist)
{
    picker->mode = mode;
    picker->hist = hist;
    picker->pos = pos;
    picker->Cur = picker->last = picker->move;
    picker->lastBad = picker->move + MAX_MOVES-1;
    picker->killers[0].move = ss->killers[0];
    picker->killers[1].move = ss->killers[1];
    
    if (pvMove)
    {
        if (mode == MP_QSEARCH && !position_is_cap_or_prom(pos, pvMove))
            picker->pvMove = MOVE_NONE;
        else
        {
            picker->pvMove = pvMove;
            picker->last++;
        }
    }
    else 
        picker->pvMove = MOVE_NONE;
}


static void move_pick_score_capture(move_pick_t* picker)
{
    position_t* pos = picker->pos;
    move_stack_t* cur;

    for (cur = picker->move; cur != picker->last; cur++)
    {
        Move move = cur->move;
        cur->score =
            PieceValue[pos->board[to_sq(move)]][PHASE_MG] -
            PieceValue[pos->board[from_sq(move)]][PHASE_MG];

        if (type_of(move) == Promotion)
            cur->score += PieceValue[promotionPiece(move)][PHASE_MG];
    }
}

static void move_pick_score_noncapture(move_pick_t* picker)
{
    position_t* pos = picker->pos;
    move_stack_t* cur;

    for (cur = picker->move; cur != picker->last; cur++)
    {
        Move move = cur->move;
        cur->score = history_value(picker->hist, pos->to_move,
                pos->board[from_sq(move)], to_sq(move));
    }
}

static void move_pick_score_evasion(move_pick_t* picker)
{
    if (picker->move + 2 > picker->last)
        return;
        
    position_t* pos = picker->pos;
    move_stack_t* cur;

    for (cur = picker->move; cur != picker->last; cur++)
    {
        Move move = cur->move;
        int see = position_see_sign(pos, move);
        if (see < 0)
            cur->score = see - HIST_MAX_VAL;
        else if (position_is_cap(pos, move))
            cur->score =
                PieceValue[pos->board[to_sq(move)]][PHASE_MG] -
                PieceValue[pos->board[from_sq(move)]][PHASE_MG] + HIST_MAX_VAL;
        else
            cur->score = history_value(picker->hist, pos->to_move,
                    pos->board[from_sq(move)], to_sq(move));
    }
}

static move_stack_t* move_pick_best(move_stack_t* first, move_stack_t* last)
{
    move_stack_t* best = first;
    move_stack_t* cur = NULL;

    for (cur = best + 1; cur != last; cur++)
    {
        if (best->score < cur->score)
            best = cur;
    }
    if (best != first)
    {
        move_stack_t temp = *first;
        *first = *best;
        *best = temp;
    }
    return first;
}

static void move_pick_advance(move_pick_t* picker)
{
    picker->Cur = picker->move;
    picker->last = picker->Cur;
    picker->mode++;
    
    switch (picker->mode)
    {
    case S1_CAPTURE:
    case S3_CAPTURE:
        picker->last = move_generate_capture(picker->pos, picker->last);
        move_pick_score_capture(picker);
        return;
    case S1_KILLER:
        picker->Cur = picker->killers;
        picker->last = picker->Cur + 2;
        return;
    case S1_BAD_CAPTURE:
        picker->Cur = picker->move + MAX_MOVES-1;
        picker->last = picker->lastBad;
        return;
    case S1_QUIET:
        picker->last = move_generate_quiet(picker->pos, picker->last);
        move_pick_score_noncapture(picker);
        return;
    case S2_EVASION:
        picker->last = move_generate_evasion(picker->pos, picker->last);
        move_pick_score_evasion(picker);
        return;
    case MP_MSEARCH:
    case MP_ESEARCH:
    case MP_QSEARCH:
        picker->mode = MP_STOP;
    case MP_STOP:
        picker->last = picker->Cur + 1;
        return;
    default:
        ASSERT_IF_FAIL(0, "invalid path");
    }
}

Move move_pick_next(move_pick_t* picker)
{
    Move move;

    while (1)
    {
        while (picker->Cur == picker->last)
            move_pick_advance(picker);
       
        switch (picker->mode)
        {
        case MP_MSEARCH:
        case MP_ESEARCH:
        case MP_QSEARCH:
            picker->Cur++;
            return picker->pvMove;
        case S1_CAPTURE:
            move = move_pick_best(picker->Cur++, picker->last)->move;
            if (move != picker->pvMove)
            {
                if (position_see_sign(picker->pos, move) >= 0)
                    return move;
                (picker->lastBad--)->move = move;
            }
            break;
        case S1_KILLER:
            move = (picker->Cur++)->move;
            if (move != MOVE_NONE &&
                move != picker->pvMove &&
                !position_is_cap(picker->pos, move) &&
                position_move_is_pseudo(picker->pos, move))
                return move;
            break;
        case S1_QUIET:
            move = move_pick_best(picker->Cur++, picker->last)->move;
            if (move != picker->pvMove &&
                move != picker->killers[0].move &&
                move != picker->killers[1].move)
                return move;
            break;
        case S1_BAD_CAPTURE:
            return (picker->Cur--)->move;
        case S2_EVASION:
            move = move_pick_best(picker->Cur++, picker->last)->move;
            if (move != picker->pvMove)
                return move;
            break;
        case S3_CAPTURE:
            return move_pick_best(picker->Cur++, picker->last)->move;
            if (move != picker->pvMove)
                return move;
        case MP_STOP:
            return MOVE_NONE;
        default:
            ASSERT_IF_FAIL(0, "invalid path");
            return MOVE_NONE;
        }
    }
}
