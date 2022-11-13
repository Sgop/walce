#include <string.h>
#include "proto_uci.h"
#include "position.h"
#include "log.h"
#include "utils.h"
#include "table.h"
#include "search.h"
#include "stats.h"
#include "interface.h"
#include "tcontrol.h"
#include "thread.h"

static int Depth;

static void info_depth(int depth)
{
    Depth = depth;
}

static void info_pv(int score, move_t* pv)
{
    char str[1024];
    char* pos = str;
    move_t* move;
    int ms = TC_get_time();
    uint64_t nodes = stats_get(ST_NODE);
    
    for (move = pv; *move != MOVE_NONE; move++)
    {
        pos += sprintf(pos, " %s", move_format(*move));
    }

    send_line("info depth %d score cp %d time %d nodes %lld nps %lld pv%s",
              Depth, score * 100 / PieceValue[P_PAWN][PHASE_MG], ms,
              nodes, ms > 0 ? nodes * 1000 / ms : nodes, str);
}

static void info_curmove(move_t move, int num)
{
    if (TC_get_time() > 3000)
        send_line("info currmove %s currmovenumber %d", move_format(move), num);
}
 
static void search_done(position_t* pos, move_t move)
{
    log_line("search_done");
    //FIXME: enter critical section
    if (!0)
    {
        position_move(pos, move);
        position_print(pos, C_WHITE);
        log_line("bestmove %s", move_format(move));
        send_line("bestmove %s", move_format(move));
    }
}
 
void loop_uci()
{
    position_t* pos = position_new();
    
    IF.info_depth = info_depth;
    IF.info_pv = info_pv;
    IF.info_curmove = info_curmove;
    IF.search_done = search_done;

    log_set_mode(MODE_GUI);
    log_line("uci mode");
    send_line("id name Walce");
    send_line("id author Markus Lausser");
    send_line("uciok");

    threads_init(pos);
    
    while (1)
    {
        char* line = get_line();
        char* token = arg_start(line);
        
        if (!token || *token == 0)
        {
            continue;
        }
        else if (!strcmp(token, "debug"))
        {
            send_line("not yet implemented");
        }
        else if (!strcmp(token, "quit"))
        {
            threads_search_stop();
            break;
        }
        else if (!strcmp(token, "isready"))
        {
            send_line("readyok");
        }
        else if (!strcmp(token, "ucinewgame"))
        {
            threads_search_stop();
            TT_clear();
        }
        else if (!strcmp(token, "position"))
        {
            char* token = arg_next();

            if (!token)
            {
                log_error("invalid position type");
                continue;
            }
            if (!strcmp(token, "startpos"))
            {
                position_reset(pos);
            }
            else if (!strcmp(token, "fen"))
            {
                token = arg_until("moves");
                if (!token)
                {
                    log_error("missing fen string");
                    continue;
                }
                position_set(pos, token);
            }
            
            token = arg_next();
            if (token && !strcmp(token, "moves"))
            {
                while ((token = arg_next()) != NULL)
                {
                    move_t move = parse_move(pos, token);
                    if (!move)
                    {
                        log_error("invalid move: %s", token);
                        continue;
                    }
                    position_move(pos, move);
                }
            }
        }
        else if (!strncmp(line, "go", 2))
        {
            TC_clear();
            while (1)
            {
                char* val;
                
                token = arg_next();

                if (token && !strcmp(token, "infinite"))
                {
                    TC.infinite = 1;
                    break;
                }

                val = arg_next();
                if (!val)
                    break;
                    
                if (!strcmp(token, "wtime"))
                {
                    if (pos->to_move == C_WHITE)
                        TC.ctime[0] = atoi(val);
                    else
                        TC.otime[0] = atoi(val);
                }
                else if (!strcmp(token, "winc"))
                {
                    if (pos->to_move == C_WHITE)
                        TC.ctime[1] = atoi(val);
                    else
                        TC.otime[1] = atoi(val);
                }
                else if (!strcmp(token, "btime"))
                {
                    if (pos->to_move == C_BLACK)
                        TC.ctime[0] = atoi(val);
                    else
                        TC.otime[0] = atoi(val);
                }
                if (!strcmp(token, "binc"))
                {
                    if (pos->to_move == C_BLACK)
                        TC.ctime[1] = atoi(val);
                    else
                        TC.otime[1] = atoi(val);
                }
                else if (!strcmp(token, "movestogo"))
                {
                    TC.togo = atoi(val);
                }
                else if (!strcmp(token, "depth"))
                {
                    TC.l_depth = atoi(val);
                }
                else if (!strcmp(token, "nodes"))
                {
                    TC.l_nodes = atoi(val);
                }
                else if (!strcmp(token, "mate"))
                {
                    //ignore for now
                }
                else if (!strcmp(token, "movetime"))
                {
                    TC.l_time = atoi(val);
                }
            }
            threads_search();
        }
        else if (!strcmp(token, "stop"))
        {
            threads_search_stop();
        }
        else
        {
            send_line("Error (unknown command): %s", line);
        }
    }
    
    position_destroy(pos);
}

