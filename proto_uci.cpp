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

using namespace walce;

static int Depth;

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
  uint64_t nodes = stats_get(ST_NODE);

  for (move = pv; *move != MOVE_NONE; move++)
  {
    pos += sprintf(pos, " %s", move_format(*move));
  }

  send_line("info depth %d score cp %d time %d nodes %lld nps %lld pv%s",
    Depth, score * 100 / PieceValue[Pawn][PHASE_MG], ms,
    nodes, ms > 0 ? nodes * 1000 / ms : nodes, str);
}

static void info_curmove(Move move, int num)
{
  if (TC.elapsed() > std::chrono::milliseconds(3000))
    send_line("info currmove %s currmovenumber %d", move_format(move), num);
}

static void search_done(position_t* pos, Move move)
{
  log_line("search_done");
  //FIXME: enter critical section
  if (!0)
  {
    position_move(pos, move);
    position_print(pos, White);
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
  send_line("id name Walce 1.0.0.1");
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
          Move move = parse_move(pos, token);
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
      TC.reset();
      while (1)
      {
        char* val;

        token = arg_next();

        if (token && !strcmp(token, "infinite"))
        {
          TC.setInfinite(true);
          break;
        }

        val = arg_next();
        if (!val)
          break;

        if (!strcmp(token, "wtime"))
        {
          TC.setTime(pos->to_move, std::chrono::milliseconds(atoi(val)));
        }
        else if (!strcmp(token, "winc"))
        {
          TC.setIncTime(pos->to_move, std::chrono::milliseconds(atoi(val)));
        }
        else if (!strcmp(token, "btime"))
        {
          TC.setTime(pos->to_move, std::chrono::milliseconds(atoi(val)));
        }
        if (!strcmp(token, "binc"))
        {
          TC.setIncTime(pos->to_move, std::chrono::milliseconds(atoi(val)));
        }
        else if (!strcmp(token, "movestogo"))
        {
          //FIXME: TC.togo = atoi(val);
        }
        else if (!strcmp(token, "depth"))
        {
          TC.setMoveDepth(atoi(val));
        }
        else if (!strcmp(token, "nodes"))
        {
          TC.setMoveNodes(atoi(val));
        }
        else if (!strcmp(token, "mate"))
        {
          //ignore for now
        }
        else if (!strcmp(token, "movetime"))
        {
          TC.setMoveTime(std::chrono::milliseconds(atoi(val)));
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

