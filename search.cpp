#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "position.h"
#include "search.h"
#include "move.h"
#include "stats.h"
#include "log.h"
#include "movepick.h"
#include "search.h"
#include "history.h"
#include "table.h"
#include "eval.h"
#include "interface.h"
#include "tcontrol.h"

#define DO_QUIECENCE   1
#define DO_ASPIRATION  0
#define DO_RAZOR       0
#define DO_FUTILITY    0
#define DO_TT          1
#define DO_TT_EXT      0
#define DO_TT2         0
#define DO_KILLERS     0
#define LOG_DEPTH      0


#if LOG_DEPTH > 0
#define LOG(fmt, arg) \
    if (ss->ply < LOG_DEPTH) log_depth(ss->ply, fmt, arg, alpha, beta);
#define LOG_EX(fmt, arg, a, b) \
    if (ss->ply < LOG_DEPTH) log_depth(ss->ply, fmt, arg, a, b);
#else
#define LOG(fmt, arg)
#define LOG_EX(fmt, arg, a, b)
#endif


static move_stack_t RootMove[MAX_MOVES];
static move_stack_t* RootMoveEnd;
static Move PV[MAX_PLY_PLUS_2];
static history_t H;


static move_stack_t* root_move_find(Move move)
{
    move_stack_t* cur = RootMove;
    for (cur = RootMove; cur != RootMoveEnd; cur++)
    {
        if (cur->move == move)
            return cur;
    }
    return NULL;
}

static int value_from_tt(int v, int ply)
{
    return  v == V_NONE      ? V_NONE
          : v >= V_MATE_PLY  ? v - ply
          : v <= -V_MATE_PLY ? v + ply
          : v;
}

static int value_to_tt(int v, int ply)
{
    return  v >= V_MATE_PLY  ? v + ply :
            v <= -V_MATE_PLY ? v - ply :
            v;
}

static void extract_pv_from_tt(position_t* pos, Move move, int depth)
{
    tentry_t* tte;
    int ply = 0;

    while (move != MOVE_NONE)
    {
        PV[ply++] = move;
        position_move(pos, move);
        tte = TT_probe(pos->state->key);
        if (tte)
        {
            move = tte->move;
            if (!position_move_is_legal(pos, move) ||
                ply >= depth ||
                (position_is_draw(pos, 1, 1) && ply >= 2))
                break;
        }
        else
            break;
    }

    PV[ply] = MOVE_NONE;
    
    while (ply--)
        position_unmove(pos);
}

int razor_margin(int d)
{
    return 512 + 16 * d;
}

static void sort_root_moves(void)
{
    move_stack_t* moves;
    
    for (moves = RootMove ; moves != RootMoveEnd; moves++)
    {
        move_stack_t* best = moves;
        move_stack_t* i2;
        for (i2 = moves + 1; i2 != RootMoveEnd; i2++)
        {
            if (best->score < i2->score)
                best = i2;
        }
        if (best != moves)
        {
            move_stack_t temp = *best;
            for (i2 = best; i2 != moves; i2--)
                *i2 = *(i2-1);
            *moves = temp;
        }
    }
}

static tentry_t* find_tentry(position_t* pos)
{
    tentry_t* tte = TT_probe(pos->state->key);
    if (tte && (!tte->move || position_move_is_pseudo(pos, tte->move)))
        return tte;
    return NULL;
}

static void add_killer(search_stack_t* ss, Move move)
{
#if DO_KILLERS
    if (move != ss->killers[0])
    {
        ss->killers[1] = ss->killers[0];
        ss->killers[0] = move;
    }
#endif
}


static int qsearch_do(
        position_t* pos, search_stack_t* ss, int isPV,
        int alpha, int beta, int depth)
{
    int inCheck = position_in_check(pos);
    tentry_t* tte;
    Move ttMove = MOVE_NONE;
    int ttValue;
    int ttDepth;
    Move move;
    move_pick_t picker;
#if DO_TT2
    state_t* state = pos->state;
    int oldAlpha = alpha;
#endif
    Move bestMove;
    int bestValue = -V_INF;
    
    if (!DO_QUIECENCE)
        return ss->sValue = eval_static(pos);
    
    ss->ply = (ss-1)->ply + 1;

    if (ss->ply >= MAX_PLY || position_is_draw(pos, 0, 0))
    {
        LOG("..q-draw in ply %d [%d %d]", ss->ply);
        return V_DRAW;
    }

#if DO_TT_EXT || DO_TT2
    tte = find_tentry(pos);
#endif
#if DO_TT_EXT
    ttMove = tte ? tte->move : MOVE_NONE;
    ttValue = tte ? value_from_tt(tte->value, ss->ply) : V_NONE;
    //ttDepth = inCheck || depth >= D_QS_CHECKS ? D_QS_CHECKS : D_QS_NO_CHECKS;
    ttDepth = D_QS_CHECKS;

    if (tte && tte->depth >= ttDepth &&
        (isPV ? tte->bound == B_EXACT :
         ttValue >= beta ? tte->bound & B_LOWER :
         tte->bound & B_UPPER))
    {
        return ttValue;
    }
#endif

    if (inCheck)
    {
        ss->sValue = V_NONE;
        bestValue = -V_INF;
        move_pick_init(&picker, pos, ss, MP_ESEARCH, ttMove, &H);
    }
    else
    {
#if DO_TT2
        if (tte)
            ss->sValue = bestValue = tte->sValue;
        else
#endif
            ss->sValue = bestValue = eval_static(pos);
        
        if (bestValue >= beta)
        {
#if DO_TT2
            if (!tte)
                TT_store(state->key, value_to_tt(bestValue, ss->ply), B_LOWER,
                         D_NONE, MOVE_NONE, ss->sValue);
#endif
            return bestValue;
        }
        
        if (isPV && bestValue > alpha)
            alpha = bestValue;
            
        move_pick_init(&picker, pos, ss, MP_QSEARCH, ttMove, &H);
    }
    
    position_get_checkinfo(pos);

    while ((move = move_pick_next(&picker)) != 0)
    {
            
        //log_line("quiet %s (%d %d)", move_format(moves[i1]), alpha, beta);
        //state_t state;
#if DO_FUTILITY
/*
        if (!isPV && !inCheck && move != ttMove &&
            type_of(move) != Promotion)
            // !givesCheck && 
            // enoughMAterial &&
            // !pos_is_passed_pawn_push 
        {
            int fBase = ss->sValue + PieceValue[Pawn][PHASE_EG] * 2;
            int fValue = fBase + PieceValue[pos->board[to_sq(move)]][PHASE_EG];
            if (type_of(move) == EnPassant)
                fValue += PieceValue[Pawn][PHASE_EG];
            if (fValue < beta)
            {
                if (fValue > bestValue)
                    bestValue = fValue;
                continue;
            }   
        }
*/
#endif
        if (!isPV && !inCheck && move != ttMove &&
          type_of(move) != Promotion &&
            position_see_sign(pos, move) < 0)
            continue;

        if (!position_pseudo_is_legal(pos, move))
            continue;

        position_move(pos, move);
        LOG("quiet %s [%d %d]", move_format(move));
        int val = -qsearch_do(pos, ss+1, isPV, -beta, -alpha, depth-1);
        position_unmove(pos);

        if (val > bestValue)
        {
            bestValue = val;

            if (val > alpha)
            {
                bestMove = move;
                if (isPV && val < beta)
                {
                    LOG("..q-pv %d [%d %d]", val);
                    alpha = val;
                }
                else
                {
#if DO_TT2
                    TT_store(state->key, value_to_tt(bestValue, ss->ply), B_LOWER,
                             ttDepth, move, ss->sValue);
#endif
                    LOG("..q-cut %d [%d %d]", val);
                    return val;
                }
            }
        }
    }

    if (bestValue == -V_INF && inCheck)
    {
        LOG("..q-return mate %d [%d %d]", -V_MATE_IN(ss->ply));
        return -V_MATE_IN(ss->ply);
    }

#if DO_TT2
    TT_store(state->key, value_to_tt(bestValue, ss->ply),
             isPV && bestValue > oldAlpha ? B_EXACT : B_UPPER,
             ttDepth, bestMove, ss->sValue);
#endif
    LOG("..q-return %d [%d %d]", bestValue);
    return bestValue;
}

/*****************************************************************/
/*****************************************************************/
static int isearch_do(
        position_t* pos, search_stack_t* ss, int isPV,
        int alpha, int beta, int depth)
{
    int inCheck = position_in_check(pos);
    Move playedMoves[64];
    int playedCnt = 0;
    tentry_t* tte;
    Move ttMove;
    int ttValue;
    move_pick_t picker;
    state_t* state = pos->state;
    Move move;
    Move bestMove = MOVE_NONE;
    int bestValue = -V_INF;
    unsigned moveCnt = 0;
    
    //log_line("ply %d, depth %d (%d %d)", ss->ply, depth, alpha, beta);
    ss->ply = (ss-1)->ply + 1;
    (ss+2)->killers[0] = (ss+2)->killers[1] = MOVE_NONE;

    if (TC.stop || ss->ply >= MAX_PLY || position_is_draw(pos, 1, isPV))
    {
        LOG("..i-draw in ply %d [%d %d]", ss->ply);
        return V_DRAW;
    }

    // check if a shorter mate (than the potential mate in this node)
    // was already found upward in the tree, and abort search in case
    if (alpha >= -V_MATE && alpha < -V_MATE_IN(ss->ply))
        alpha = -V_MATE_IN(ss->ply);
    if (beta <= V_MATE && beta > V_MATE_IN(ss->ply+1))
        beta = V_MATE_IN(ss->ply+1);
    if (alpha >= beta)
        return alpha;

    tte = find_tentry(pos);
    ttMove = tte ? tte->move : MOVE_NONE;
    ttValue = tte ? value_from_tt(tte->value, ss->ply) : V_NONE;

    if (tte && tte->depth >= depth && ttValue != V_NONE &&
        (isPV ? tte->bound == B_EXACT :
         ttValue >= beta ? tte->bound & B_LOWER :
         tte->bound & B_UPPER))
    {   
        LOG("..i-touch tt %s [%d %d]", move_format(ttMove));
        TT_touch(tte);
        if (ttValue >= beta && ttMove && !position_is_cap_or_prom(pos, ttMove))
        {
            add_killer(ss, ttMove);
        }
        LOG_EX("..i-return tte %d [%d %d]", ttValue, tte->depth, tte->bound);
        return ttValue;
    }

    if (inCheck)
    {
        ss->sValue = V_NONE;
        move_pick_init(&picker, pos, ss, MP_ESEARCH, ttMove, &H);
    }
    else
    {
        if (tte)
        {
            ss->sValue = tte->sValue;
        }
        else
        {
            ss->sValue = eval_static(pos);
#if DO_TT2
            TT_store(state->key, V_NONE, B_NONE, D_NONE, MOVE_NONE, ss->sValue);
#endif
        }
        move_pick_init(&picker, pos, ss, MP_MSEARCH, ttMove, &H);
    }

#if DO_RAZOR
    if (depth < 4 && !inCheck &&
        ss->sValue + razor_margin(depth) < beta &&
        ttMove == MOVE_NONE &&
        abs(beta) < V_MATE_PLY &&
        !position_pawns_on7(pos, pos->to_move) )
    {
        int rbeta = beta - razor_margin(depth);
        int value = search_do_quiescence(pos, ss, N_NON_PV, rbeta-1, rbeta);
        if (value < rbeta)
            return value;
    }
#endif

    position_get_checkinfo(pos);

    while ((move = move_pick_next(&picker)) != 0)
    {
        int val = -V_INF;
        int isCapOrProm;
        int pvMove;
        
#if DO_FUTILITY
        if (!isCapOrProm &&
            !inCheck &&
//            !dangerous && 
            move != ttMove &&
            (bestValue > -V_MATE_PLY || (bestValue == -V_INF && alpha > V_MATE_PLY))
            )
        {
            ;
        }
#endif
        if (!position_pseudo_is_legal(pos, move))
            continue;

        position_move(pos, move);
        moveCnt++;
            
        isCapOrProm = position_is_cap_or_prom(pos, move);
        if (!isCapOrProm && playedCnt < 64)
            playedMoves[playedCnt++] = move;

#if DO_LMR
        if (depth > 3 &&
            !pvSearch)
        {
        }
#endif

        pvMove = isPV ? moveCnt == 1 : 0;
        
        if (!pvMove)
        {
            LOG_EX("i-null %s [%d %d]", move_format(move), alpha, alpha+1);
            val = depth <= 1 ?
                -qsearch_do(pos, ss+1, 0, -alpha-1, -alpha, D_ZERO) :
                -isearch_do(pos, ss+1, 0, -alpha-1, -alpha, depth-1);
        }

        if (pvMove || (val > alpha && val < beta))
        {
            LOG("i-full %s [%d %d]", move_format(move));
            val = depth <= 1 ?
                -qsearch_do(pos, ss+1, 1, -beta, -alpha, D_ZERO) :
                -isearch_do(pos, ss+1, 1, -beta, -alpha, depth-1);
        }

        position_unmove(pos);

        if (TC.stop)
            return val;
            
        if (val > bestValue)
        {
            bestValue = val;
            
            if (val > alpha)
            {
                bestMove = move;

                if (isPV && val < beta)
                {
                    LOG("..i-pv %d [%d %d]", val);
                    alpha = val;
                }
                else
                {
                    LOG("..i-cut %d [%d %d]", val);
                    break;
                }
            }
        }
    }

    if (moveCnt == 0)
    {
        if (inCheck)
        {
            LOG("..i-return mate %d [%d %d]", -V_MATE_IN(ss->ply));
            return -V_MATE_IN(ss->ply);
        }
        else
        {
            LOG("..i-return draw %d [%d %d]", V_DRAW);
            return V_DRAW;
        }
    }

    if (bestValue == -V_INF)
        bestValue = alpha;

    if (bestValue >= beta)
    {
#if DO_TT
        LOG_EX("..istore lower %s %d %d", move_format(bestMove), value_to_tt(bestValue, ss->ply), depth);
        TT_store(state->key, value_to_tt(bestValue, ss->ply), B_LOWER, depth, bestMove, ss->sValue);
#endif
        if (!position_is_cap_or_prom(pos, bestMove))
        {
            int bonus = depth * depth;
                    
            add_killer(ss, bestMove);
            history_add(&H, pos->to_move, pos->board[from_sq(bestMove)], to_sq(bestMove), bonus);

            int i1;
            for (i1 = 0; i1 < playedCnt-1; i1++)
            {
                Move m = playedMoves[i1];
                history_add(&H, pos->to_move, pos->board[from_sq(m)], to_sq(m), -bonus);
            }
        }
    }
#if DO_TT
    else
    {
        if (bestMove == MOVE_NONE)
        {
            LOG_EX("..istore upper %s %d %d", "none", value_to_tt(bestValue, ss->ply), depth);
            TT_store(state->key, value_to_tt(bestValue, ss->ply), 
                     B_UPPER, depth, bestMove, ss->sValue);
        }
        else
        {
            LOG_EX("..istore exact %s %d %d", move_format(bestMove), value_to_tt(bestValue, ss->ply), depth);
            TT_store(state->key, value_to_tt(bestValue, ss->ply), 
                     B_EXACT, depth, bestMove, ss->sValue);
        }
    }
#endif
    LOG("..i-return %d [%d %d]", bestValue);
    return bestValue;
}


/*****************************************************************/
/*****************************************************************/
static int rsearch_do(
        position_t* pos, search_stack_t* ss,
        int alpha, int beta, int depth)
{
    int inCheck = position_in_check(pos);
    Move playedMoves[64];
    int playedCnt = 0;
    move_pick_t picker;
    state_t* state = pos->state;
    Move move;
    Move ttMove;
    Move bestMove = MOVE_NONE;
    int bestValue = -V_INF;
    unsigned moveCnt = 0;
    
    ss->ply = 0;
    (ss+2)->killers[0] = (ss+2)->killers[1] = MOVE_NONE;

    //log_line("ply %d, depth %d (%d %d)", ss->ply, depth, alpha, beta);
    ASSERT_IF_FAIL(RootMove < RootMoveEnd, "have no moves");
    
    ttMove = RootMove->move;

    if (inCheck)
    {
        ss->sValue = V_NONE;
        move_pick_init(&picker, pos, ss, MP_ESEARCH, ttMove, &H);
    }
    else
    {
        ss->sValue = eval_static(pos);
#if DO_TT2
        TT_store(state->key, V_NONE, B_NONE, 0, MOVE_NONE, ss->sValue);
#endif
        move_pick_init(&picker, pos, ss, MP_MSEARCH, ttMove, &H);
    }

    while ((move = move_pick_next(&picker)) != 0)
    {
        int isCapOrProm;
        int val = -V_INF;
        move_stack_t* rMove = root_move_find(move);
        int pvMove;
        
        if (!rMove)
            continue;

        position_move(pos, move);
        moveCnt++;
        IF.info_curmove(move, moveCnt);

        isCapOrProm = position_is_cap_or_prom(pos, move);
        if (!isCapOrProm && playedCnt < 64)
            playedMoves[playedCnt++] = move;

#if DO_LMR
        if (depth > 3 &&
            !pvSearch)
        {
        }
#endif

        pvMove = (moveCnt == 1);
        
        if (!pvMove)
        {
            LOG_EX("r-null %s [%d %d]", move_format(move), alpha, alpha+1);
            val = depth <= 1 ?
                -qsearch_do(pos, ss+1, 0, -alpha-1, -alpha, D_ZERO) :
                -isearch_do(pos, ss+1, 0, -alpha-1, -alpha, depth-1);
        }

        if (pvMove || val > alpha)
        {
            LOG("r-full %s [%d %d]", move_format(move));
            val = depth == 1 ?
                -qsearch_do(pos, ss+1, 1, -beta, -alpha, D_ZERO) :
                -isearch_do(pos, ss+1, 1, -beta, -alpha, depth-1);
        }

        position_unmove(pos);

        if (TC.stop)
            return val;

        rMove->score = -V_INF;
        
        if (val > bestValue)
        {
            bestValue = val;

            if (val > alpha)
            {
                alpha = val;
                bestMove = move;
                rMove->score = val;
                extract_pv_from_tt(pos, move, depth);

                if (val < beta)
                {
                    LOG("..pv %d [%d %d]", val);
                }
                else
                {
                    LOG("..cut %d [%d %d]", val);
                    break;
                }
            }
        }
    }
    
    if (bestValue >= beta)
    {
#if DO_TT
        LOG_EX("..rstore lower %s %d %d", move_format(bestMove), value_to_tt(bestValue, ss->ply), depth);
        TT_store(state->key, value_to_tt(bestValue, ss->ply), B_LOWER, depth, bestMove, ss->sValue);
#endif
        if (!position_is_cap_or_prom(pos, bestMove) && !inCheck)
        {
            int bonus = depth * depth;
                    
            add_killer(ss, bestMove);
            history_add(&H, pos->to_move, pos->board[from_sq(bestMove)], to_sq(bestMove), +bonus);

            int i1;
            for (i1 = 0; i1 < playedCnt - 1; i1++)
            {
                Move m = playedMoves[i1];
                history_add(&H, pos->to_move, pos->board[from_sq(m)], to_sq(m), -bonus);
            }
        }
    }
#if DO_TT
    else
    {
        if (bestMove == MOVE_NONE)
        {
            LOG_EX("..rstore upper %s %d %d", "none", value_to_tt(bestValue, ss->ply), depth);
            TT_store(state->key, value_to_tt(bestValue, ss->ply),
                     B_UPPER, depth, bestMove, ss->sValue);
        }
        else
        {
            LOG_EX("..rstore exact %s %d %d", move_format(bestMove), value_to_tt(bestValue, ss->ply), depth);
            TT_store(state->key, value_to_tt(bestValue, ss->ply),
                     B_EXACT, depth, bestMove, ss->sValue);
        }
    }
#endif
    LOG("..return %d [%d %d]", bestValue);
    return bestValue;
}


/*****************************************************************/
/*****************************************************************/
Move search_do(position_t* pos)
{
    search_stack_t ss[MAX_PLY_PLUS_2];
    int depth = 0;
    int maxPly = (TC.l_depth == 0 || TC.l_depth > MAX_PLY) ? MAX_PLY : TC.l_depth;

    history_clear(&H);
    memset(RootMove, 0, MAX_MOVES * sizeof(*RootMove));
    memset(PV, 0, MAX_PLY_PLUS_2 * sizeof(*PV));
    memset(ss, 0, 4 * sizeof(*ss));
    TT_advance();
    
    RootMoveEnd = move_generate_legal(pos, RootMove);

    // immediate return if no move possible
    if (RootMove == RootMoveEnd)
        return MOVE_NONE;
    
    if (RootMove+1 == RootMoveEnd)
        return RootMove->move;
    
    while (!TC.stop && ++depth <= maxPly)
    {
        int alpha, beta, delta;
        int bestValue = -V_INF;
        IF.info_depth(depth);

#if DO_ASPIRATION
        int prevScore = RootMove->score;

        if (depth >= 5 && abs(prevScore) < V_WIN)
        {
            delta = PieceValue[Pawn][PHASE_MG] / 4;
            alpha = prevScore - delta;
            beta = prevScore + delta;
        }
        else
#endif
        {
            delta = -V_INF;  // dummy
            alpha = -V_INF;
            beta = V_INF;
        }
        
        while (1)  // aspiration window search
        {
            bestValue = rsearch_do(pos, ss, alpha, beta, depth);

            sort_root_moves();

            if (TC.stop)
                return RootMove->move;
            
            IF.info_pv(bestValue, PV);
                
            if (bestValue > alpha && bestValue < beta)
                break;
                
            if (abs(bestValue) >= V_WIN)
            {
                alpha = -V_INF;
                beta = V_INF;
            }
            else if (bestValue >= beta)
            {
                alpha = beta-1;
                beta = bestValue + delta;
                delta += delta / 2;
            }
            else
            {
                beta = alpha+1;
                alpha = bestValue - delta;
                delta += delta / 2;
            }

        }
        
        if (RootMove->score != bestValue)
            log_error("invalid root move");
        if (RootMove->move != PV[0])
            log_error("invalid PV[0]");

        if (bestValue >= V_MATE_PLY)
        {
            if (V_MATE_IN(bestValue) <= depth)
            {
                log_line("set mate in %d plies", V_MATE_IN(bestValue));
                break;
            }
        }
        else if (bestValue <= -V_MATE_PLY)
        {
            if (V_MATE_IN(-bestValue) <= depth)
            {
                log_line("get mated in %d plies", V_MATE_IN(-bestValue));
                break;
            }
        }
        //if (pos->state->plyNum <= 5 && TC_get_time() >= 1000 * (pos->state->plyNum+1))
        //    break;

        if (!TC_have_more_time())
            break;
    }
    log_line("info_done");
    IF.info_done();

    return RootMove->move;
}

Move think(position_t* pos)
{
    Move move;
    
    log_line("Thinking %s(depth: %d)(time: %d ms)(node: %d)(time: %.1f/%d s)",
       TC.infinite ? "infinite ": "", TC.l_depth, TC.l_time, TC.l_nodes,
       (float)TC.ctime[0] / 1000.0, TC.ctime[1] / 1000);

    stats_start();
    move = search_do(pos);
    stats_print();

    return move;
}


uint64_t perft(position_t* pos, int depth)
{
    move_stack_t moves[MAX_MOVES];
    move_stack_t* last;
    move_stack_t* cur;
    uint64_t result = 0;
    
#if 1
    last = move_generate_legal(pos, moves);

    if (depth <= 1)
        return (last - moves);
#else
    if (depth <= 0)
        return 1;

    last = move_generate_legal(pos, moves);
#endif
        
    for (cur = moves; cur != last; cur++)
    {
        position_move(pos, cur->move);
        result += perft(pos, depth-1);
        position_unmove(pos);
    }
    return result;
}

uint64_t divide(position_t* pos, int depth)
{
    move_stack_t moves[MAX_MOVES];
    move_stack_t* last;
    move_stack_t* cur;
    uint64_t result = 0;
    
    if (depth <= 0)
    {
        log_error("Depth <= 0 is bad idea.");
        return 0;
    }
        
    last = move_generate_legal(pos, moves);

    for (cur = moves; cur != last; cur++)
    {
        position_move(pos, cur->move);
        uint64_t num = perft(pos, depth-1);
        position_unmove(pos);
        result += num;
        log_line("%s %llu", move_format(cur->move), num);
    }
    log_line("Moves: %u", (last-moves));
    return result;
}

/**
 * Parse input move and check legality.
 * @param position the position
 * @param move the move to check and update
 * @return 1 if legal, 0 otherwise
 */
Move parse_move(position_t* pos, const char* line)
{
    if (strlen(line) < 4)
        return MOVE_NONE;
                
    File file1 = File(line[0] - 'a');
    Rank rank1 = Rank(line[1] - '1');
    File file2 = File(line[2] - 'a');
    Rank rank2 = Rank(line[3] - '1');
    char prom = line[4];
    int piece = Knight;
            
    if ((file1 >> 3) != 0 || (rank1 >> 3) != 0 ||
        (file2 >> 3) != 0 || (rank2 >> 3) != 0)
        return MOVE_NONE;
                    
    if (prom)
    {
        if (prom == 'q')
            piece = Queen;
        else if (prom == 'r')
            piece = Rook;
        else if (prom == 'b')
            piece = Bishop;
        else if (prom == 'n')
            piece = Knight;
        else if (prom == ' ')
            prom = 0;
        else
            return MOVE_NONE;
    }
    
    Square from = make_square(file1, rank1);
    Square to = make_square(file2, rank2);
    
    // make normal move
    Move m = make<NormalMove>(from, to, piece);
    
    // create legal moves and update move type
    move_stack_t moves[MAX_MOVES];
    move_stack_t* last = move_generate_legal(pos, moves);
    move_stack_t* Cur = moves;
    
    for ( ; Cur != last; Cur++)
    {
        if ((Cur->move & ~Castle) == m)
        {
            if (type_of(Cur->move) == Promotion && !prom)
                return MOVE_NONE;
            if (type_of(Cur->move) != Promotion && prom)
                return MOVE_NONE;
            return Cur->move;
        }
    }
    return MOVE_NONE;
}
