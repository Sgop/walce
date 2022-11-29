#include <string>

#include "bitboard.h"
#include "position.h"
#include "log.h"
#include "prnd.h"
#include "table.h"
#include "utils.h"
#include "proto_console.h"
#include "proto_xboard.h"
#include "proto_uci.h"
#include "interface.h"
#include "tcontrol.h"
#include "thread.h"

//static void sig_int(int arg) { } 
//static void sig_term(int arg) { }
static void info_pv(int score, Move* pv) {}
static void info_depth(int depth) {}
static void info_done() {}
static void info_curmove(Move move, int num) {}
static void search_done(position_t* pos, Move move) {}

int main(int argc, char** argv)
{
  bitboards_init();
  positions_init();
  log_init();
  tables_init();
  prnd_init();

  //signal(SIGINT, sig_int);
  //signal(SIGINT, sig_term);

  IF.info_pv = info_pv;
  IF.info_depth = info_depth;
  IF.info_done = info_done;
  IF.info_curmove = info_curmove;
  IF.search_done = search_done;

  while (1)
  {
    char* line = nullptr;
    char* token = nullptr;
    if (argc < 2)
    {
      line = get_line();
      token = arg_start(line);
    }

    if (!token || std::string(token) == "console")
    {
      loop_console(argv[1]);
    }
    else if (std::string(token) == "xboard")
    {
      loop_xboard();
    }
    else if (std::string(token) == "uci")
    {
      loop_uci();
    }
    else if (std::string(token) == "quit")
    {
    }
    else
    {
      log_line("specifiy interface mode: [xboard | uci | console]");
      continue;
    }
    break;
  }

  log_exit();
  tables_exit();

  return 0;
}
