#include <string.h>
#include <stdlib.h>
#include "proto_console.h"
#include "position.h"
#include "log.h"
#include "utils.h"
#include "table.h"
#include "search.h"
#include "stats.h"
#include "interface.h"
#include "eval.h"
#include "tcontrol.h"

using namespace walce;

static int Depth = 0;
static Color Us = White;
static char Str[1024];

static void info_depth(int depth)
{
  Depth = depth;
}

static void info_done()
{
  if (*Str)
  {
    log_line(Str);
    *Str = 0;
  }
}

static void info_pv(int score, Move* pv)
{
  char* pos = Str;
  Move* move;
  auto ms = TC.elapsed().count();

  pos += sprintf(pos, "%2u  %7d  %10.3f  %8d  ",
    Depth, score, (double)ms / 1000, stats_get(ST_NODE));

  for (move = pv; *move != MOVE_NONE; move++)
  {
    pos += sprintf(pos, " %s", move_format(*move));
  }

  if (ms < 0)
    return;

  log_line(Str);
  *Str = 0;
}


void loop_console(const char* fen)
{
  position_t* pos = position_new();

  IF.info_depth = info_depth;
  IF.info_pv = info_pv;
  IF.info_done = info_done;

  log_set_mode(0);
  log_line("console mode");
  if (fen)
    position_set(pos, fen);
  position_print(pos, Us);

  TC.setMoveTime(std::chrono::milliseconds(1000));

  while (1)
  {
    char* line = get_line();
    char* token = arg_start(line);

    if (!token || *token == 0)
    {
      continue;
    }
    else if (!strcmp(token, "help"))
    {
      log_line("  quit            - quit the program");
      log_line("  new             - start new game");
      log_line("  print           - print board");
      log_line("  + [num]         - let engine do <num> moves");
      log_line("  ++ [depth]      - let enigne search for best move");
      log_line("  <move>          - do user move");
      log_line("  set <FEN>       - set board using FEN string");
      log_line("  get             - get the FEN string");
      log_line("  perft <depth>   - do performance test for move generator");
      log_line("  divide <depth>  - same as perft, but prints number of subnodes");
      log_line("  calc <static | see <move> | check>");
      log_line("  depth <depth>   - set search depth (0=time limit)");
      log_line("  time <time>     - set time limit (when depth = 0)");
      log_line("  undo            - undo move");
      log_line("  check           - check internal variables");
      log_line("  ttclear         - ");
      log_line("  flip            - flip board");
    }
    else if (!strcmp(token, "quit"))
    {
      break;
    }
    else if (!strcmp(token, "new"))
    {
      TT_clear();
      position_reset(pos);
      position_print(pos, Us);
    }
    else if (!strcmp(token, "print"))
    {
      position_print(pos, Us);
    }
    else if (!strcmp(token, "undo"))
    {
      if (pos->state->prev)
      {
        position_unmove(pos);
        position_print(pos, Us);
      }
      else
      {
        log_error("cannot take back move");
      }
    }
    else if (!strcmp(token, "set"))
    {
      char* fen = arg_rest();
      TT_clear();
      position_set(pos, fen);
      position_print(pos, Us);
    }
    else if (!strcmp(token, "get"))
    {
      log_line("%s", position_get(pos));
    }
    else if (!strcmp(token, "perft"))
    {
      char* depth = arg_next();
      if (!depth)
      {
        log_error("missing argument");
      }
      else
      {
        TC.start();
        uint64_t nodes = perft(pos, atoi(depth));
        auto ms = TC.elapsed().count() + 1;
        log_line("Perft: %lld nodes (%.3f seconds, %llu/ms)",
          nodes, (double)ms / 1000.0, nodes / ms);
      }
    }
    else if (!strcmp(token, "divide"))
    {
      char* depth = arg_next();
      if (!depth)
      {
        log_error("missing argument");
      }
      else
      {
        TC.start();
        uint64_t nodes = divide(pos, atoi(depth));
        auto ms = TC.elapsed().count() + 1;
        log_line("Divide: %lld nodes (%.3f seconds, %llu/ms)",
          nodes, (double)ms / 1000.0, nodes / ms);
      }
    }
    else if (!strcmp(token, "++"))
    {
      char* depth = arg_next();
      Move move;

      if (depth)
        TC.setMoveDepth(std::atoi(depth));
      else
        TC.setMoveDepth(0);

      move = think(pos);
      log_line("best %s", move_format(move));
      position_print(pos, Us);
    }
    else if (!strcmp(token, "+"))
    {
      char* num = arg_next();
      int n = num ? atoi(num) : 1;

      if (n > 1)
        log_line("do %d moves", n);
      else
        n = 1;

      while (n > 0)
      {
        Move move = think(pos);
        if (move)
        {
          log_line("engine move %s", move_format(move));
          position_move(pos, move);
          position_print(pos, Us);
        }
        else
        {
          log_line("cannot move");
          break;
        }
        n--;
      }
    }
    else if (!strcmp(token, "calc"))
    {
      char* what = arg_next();
      if (!what)
      {
        log_line("calc what?");
      }
      else if (!strcmp(what, "static"))
      {
        log_line("Static value: %d", eval_static(pos));
      }
      else if (!strcmp(what, "see"))
      {
        char* m = arg_next();
        Move move = m ? parse_move(pos, m) : MOVE_NONE;
        if (!move)
          send_line("Invalid move: %s", m);
        else
          log_line("Static exchange evaluation: %d", position_see(pos, move));
      }
      else if (!strcmp(what, "check"))
      {
        position_check_vars(pos);
      }
      else
      {
        log_line("dont know '%s'", what);
      }
    }
    else if (!strcmp(token, "depth"))
    {
      char* d = arg_next();
      if (!d)
        log_error("missing argument");
      else
        TC.setMoveDepth(atoi(d));
    }
    else if (!strcmp(token, "time"))
    {
      char* t = arg_next();
      if (!t)
        log_error("missing argument");
      else
        TC.setMoveTime(std::chrono::milliseconds(atoi(t)));
    }
    else if (!strcmp(token, "ttclear"))
    {
      TT_clear();
    }
    else if (!strcmp(token, "flip"))
    {
      position_print(pos, Us = ~Us);
    }
    else if (strlen(token) >= 4)
    {
      Move move = parse_move(pos, token);
      if (!move)
      {
        log_line("invalid move");
      }
      else
      {
        log_line("human move %s", move_format(move));
        position_move(pos, move);
        position_print(pos, Us);
        if (position_is_draw(pos, 1, 1))
          log_line("Game is draw");
      }
    }
    else
    {
      log_line("Unknown command: %s", line);
    }
  }

  position_destroy(pos);
}

