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

using namespace walce;

static int ModePost = 0;
static int ModeAnalyze = 0;
static int InGame = 0;
static int Depth = 0;

static void info_depth(int depth)
{
  Depth = depth;
}

static void info_pv(int score, Move* pv)
{
  char str[1024];
  char* pos = str;
  Move* move;
  auto ms = TC.elapsed().count();

  if (!ModePost)
    return;

  pos += sprintf(pos, "%2d  %7d  %5lld  %8d  ", Depth,
    score * 100 / PieceValue[Pawn][PHASE_MG], ms / 10, stats_get(ST_NODE));

  for (move = pv; *move != MOVE_NONE; move++)
  {
    pos += sprintf(pos, " %s", move_format(*move));
  }
  if (ModePost)
    send_line(str);
  else
    log_line(str);
}

static void info_curmove(Move move, int num)
{
  if (TC.elapsed() > std::chrono::milliseconds(3000))
    log_line("%2d. %s", num, move_format(move));
}

static void search_done(position_t* pos, Move move)
{
  //FIXME: enter critical section
  if (!ModeAnalyze && InGame && move != MOVE_NONE)
  {
    position_move(pos, move);
    position_print(pos, White);
    send_line("move %s", move_format(move));
  }
}

void loop_xboard(void)
{
  position_t* pos = position_new();
  Color engineColor = Black;

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
      engineColor = Black;
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
      !strcmp(token, "computer"))
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
      !strcmp(token, "egtpath"))
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
        TC.setMoveTime(std::chrono::seconds(atoi(token)));
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

        //FIXME TC.togo = atoi(moves);
        TC.setTime(White, std::chrono::seconds(t));
        TC.setTime(Black, std::chrono::seconds(t));
        TC.setIncTime(White, std::chrono::seconds(atoi(inc)));
        TC.setIncTime(Black, std::chrono::seconds(atoi(inc)));
      }
    }
    else if (!strcmp(token, "time"))
    {
      token = arg_next();
      if (token)
        TC.setTime(White, std::chrono::milliseconds(atoi(token) * 10));
    }
    else if (!strcmp(token, "otim"))
    {
      token = arg_next();
      if (token)
        TC.setTime(Black, std::chrono::milliseconds(atoi(token) * 10));
    }
    else if (!strcmp(token, "analyze"))
    {
      ModeAnalyze = 1;
      TC.setInfinite(true);
      engineColor = pos->to_move;

      threads_search();
    }
    else if (ModeAnalyze && !strcmp(token, "exit"))
    {
      threads_search_stop();
      ModeAnalyze = 0;
      TC.setInfinite(false);
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
      engineColor = ColorNone;
    }
    else if (!strcmp(token, "go"))
    {
      engineColor = pos->to_move;
      threads_search();
    }
    else if (!strcmp(token, "playother"))
    {
      engineColor = ~pos->to_move;
    }
    else if (!strcmp(token, "white"))
    {
      pos->to_move = White;
      engineColor = Black;
    }
    else if (!strcmp(token, "black"))
    {
      pos->to_move = Black;
      engineColor = White;
    }
    else if (!strcmp(token, "sd"))
    {
      char* d = arg_next();
      if (d)
        TC.setMoveDepth(atoi(d));
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
          TC.setMoveTime(std::chrono::milliseconds(atoi(v)));
      }
      else if (!strcmp(o, "Search Depth"))
      {
        if (v)
          TC.setMoveDepth(atoi(v));
      }
      else
        log_line("unknown option: %s", o);
    }
    else
    {
      if (!strcmp(token, "usermove"))
        token = arg_next();

      threads_search_stop();

      Move move = parse_move(pos, token);
      if (!move)
      {
        send_line("Illegal move: %s", token);
      }
      else
      {
        position_move(pos, move);
        position_print(pos, White);
        if (ModeAnalyze || engineColor == pos->to_move)
          threads_search();
      }
    }
  }

  threads_exit();
  position_destroy(pos);
}

