#include <string.h>
#include <stdlib.h>
#include "proto_xboard.h"
#include "position.h"
#include "log.h"
#include "utils.h"
#include "table.h"
#include "search.h"
#include "stats.h"
#include "interface.h"
#include "tcontrol.h"
#include "thread.h"

static int ModePost = 0;
static int ModeAnalyze = 0;
static int InGame = 0;
static int Depth = 0;

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
    
    if (!ModePost)
        return;
        
    pos += sprintf(pos, "%2u  %7d  %5d  %8d  ", Depth,
                   score * 100 / PieceValue[P_PAWN][PHASE_MG], ms/10, stats_get(ST_NODE));

    for (move = pv; *move != MOVE_NONE; move++)
    {
        pos += sprintf(pos, " %s", move_format(*move));
    }
    if (ModePost)
        send_line(str);
    else
        log_line(str);
}

static void info_curmove(move_t move, int num)
{
    if (TC_get_time() > 3000)
        log_line("%2d. %s", num, move_format(move));
}
 
static void search_done(position_t* pos, move_t move)
{
    //FIXME: enter critical section
    if (!ModeAnalyze && InGame && move != MOVE_NONE)
    {
        position_move(pos, move);
        position_print(pos, C_WHITE);
        send_line("move %s", move_format(move));
    }
}
 
void loop_xboard(void)
{
    position_t* pos = position_new();
    color_t engineColor = C_BLACK;
    
    IF.info_depth = info_depth;
    IF.info_pv = info_pv;
    IF.info_curmove = info_curmove;
    IF.search_done = search_done;

    log_set_mode(MODE_GUI);
    log_line("xboard mode");
    
    threads_init(pos);
    
    while (1)
    {
        char* line = get_line();
        char* token = arg_start(line);
        
        if (!strcmp(token, "new"))
        {
            position_reset(pos);
            TT_clear();
            engineColor = C_BLACK;
            InGame = 1;
        }
        else if (!strcmp(token, "quit"))
        {
            InGame = 0;
            threads_search_stop();
            break;
        }
        else if (!strcmp(token, "protover"))
        {
            char* v = arg_next();
            if (v && atoi(v) == 2)
            {
                send_line("feature myname=\"Walce\"");
                send_line("feature setboard=1 usermove=1 sigint=0 sigterm=0");
                send_line("feature playother=1 ping=1 time=1 colors=0 name=1");
                send_line("feature ics=1 analyze=1");
                send_line("feature option=\"Search Depth -spin %d 0 20\"", 0);
                send_line("feature option=\"Thinking Time -spin %d 0 600000\"", 0);
                send_line("feature done=1");
            }
        }
        else if (!strcmp(token, "random") ||
                 !strcmp(token, "bk") ||
                 !strcmp(token, "ics") ||
                 !strcmp(token, "name") ||
                 !strcmp(token, "accepted") ||
                 !strcmp(token, "computer") )
        {
            ; // IGNORE
        }
        else if (!strcmp(token, "variant") ||
                 !strcmp(token, "rejected") ||
                 !strcmp(token, "draw") ||
                 !strcmp(token, "hint") ||
                 !strcmp(token, "hard") ||
                 !strcmp(token, "easy") ||
                 !strcmp(token, "rating") ||
                 !strcmp(token, "pause") ||
                 !strcmp(token, "resume") ||
                 !strcmp(token, "memory") ||
                 !strcmp(token, "cores") ||
                 !strcmp(token, "egtpath") )
        {
            send_line("Error (not implemented yet): %s", token);
        }
        else if (!strcmp(token, "?"))
        {
            threads_search_stop();
        }
        else if (!strcmp(token, "st"))
        {
            token = arg_next();
            if (token)
                TC.l_time = atoi(token) * 1000;
        }
        else if (!strcmp(token, "level"))
        {
            char* moves = arg_next();
            char* min = arg_next();
            char* inc = arg_next();
            char* sec = min ? strchr(min, ':') : NULL;
            
            if (sec)
                *sec++ = 0;

            if (inc)
            {
                int t = atoi(min) * 60 + (sec ? atoi(sec) : 0);

                TC.togo = atoi(moves);
                TC.ctime[0] = TC.otime[0] = t * 1000;
                TC.ctime[1] = TC.otime[1] = atoi(inc) * 1000;
            }
        }
        else if (!strcmp(token, "time"))
        {
            token = arg_next();
            if (token)
                TC.ctime[0] = atoi(token) * 10;
        }
        else if (!strcmp(token, "otim"))
        {
            token = arg_next();
            if (token)
                TC.otime[0] = atoi(token) * 10;
        }
        else if (!strcmp(token, "analyze"))
        {
            ModeAnalyze = 1;
            TC.infinite = 1;
            engineColor = pos->to_move;

            threads_search();
        }
        else if (ModeAnalyze && !strcmp(token, "exit"))
        {
            threads_search_stop();
            ModeAnalyze = 0;
            TC.infinite = 0;
        }
        else if (ModeAnalyze && !strcmp(token, "."))
        {
            //send_line("Error (not implemented yet): %s", token);
        }
        else if (!strncmp(line, "result", 6))
        {
            InGame = 0;
            threads_search_stop();
        }
        else if (!strncmp(line, "force", 5))
        {
            engineColor = C_NONE;
        }
        else if (!strcmp(token, "go"))
        {
            engineColor = pos->to_move;
            threads_search();
        }
        else if (!strcmp(token, "playother"))
        {
            engineColor = 1 ^ pos->to_move;
        }
        else if (!strcmp(token, "white"))
        {
            pos->to_move = C_WHITE;
            engineColor = C_BLACK;
        }
        else if (!strcmp(token, "black"))
        {
            pos->to_move = C_BLACK;
            engineColor = C_WHITE;
        }
        else if (!strcmp(token, "sd"))
        {
            char* d = arg_next();
            if (d)
                TC.l_depth = atoi(d);
        }
        else if (!strcmp(token, "ping"))
        {
            char* a = arg_rest();
            if (a)
                send_line("pong %s", a);
            else
                send_line("pong");
        }
        else if (!strcmp(token, "edit"))
        {
            send_line("Error (command not implemented): %s", token);
        }
        else if (!strcmp(token, "undo"))
        {
            if (!position_unmove(pos))
                send_line("Error (command not legal now): %s", token);
        }
        else if (!strcmp(token, "remove"))
        {
            if (!position_unmove(pos) || !position_unmove(pos))
                send_line("Error (command not legal now): %s", token);
        }
        else if (!strcmp(token, "setboard"))
        {
            char* b = arg_rest();
            if (!b)
                send_line("Error (missing argument): %s", token);
            else
                position_set(pos, b);
        }
        else if (!strcmp(token, "post"))
        {
            ModePost = 1;
        }
        else if (!strcmp(token, "nopost"))
        {
            ModePost = 1;
        }
        else if (!strcmp(token, "option"))
        {
            char* o = arg_next_sep('=');
            char* v = arg_next();
            if (!o)
                log_line("missing option");
            else if (!strcmp(o, "Thinking Time"))
            {
                if (v)
                    TC.l_time = atoi(v);
            }
            else if (!strcmp(o, "Search Depth"))
            {
                if (v)
                    TC.l_depth = atoi(v);
            }
            else
                log_line("unknown option: %s", o);
        }
        else
        {
            if (!strcmp(token, "usermove"))
                token = arg_next();
            
            threads_search_stop();
            
            move_t move = parse_move(pos, token);
            if (!move)
            {
                send_line("Illegal move: %s", token);
            }
            else
            {
                position_move(pos, move);
                position_print(pos, C_WHITE);
                if (ModeAnalyze || engineColor == pos->to_move)
                    threads_search();
            }
        }
    }

    threads_exit();
    position_destroy(pos);
}

