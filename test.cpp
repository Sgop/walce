#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "position.h"
#include "search.h"
#include "stats.h"
#include "platform.h"
#include "prnd.h"
#include "table.h"
#include "interface.h"
#include "tcontrol.h"

using namespace walce;

static int Depth;

static char* strip(char* str)
{
  char* result = str;

  while (isspace(*result))
  {
    result++;
  }

  auto len = strlen(result);
  while (len > 0 && isspace(result[len - 1]))
  {
    result[len - 1] = 0;
    len--;
  }
  return result;
}

static void test_start(const char* desc)
{
  printf("\n");
  printf("**************************************************\n");
  printf(" Test case: %s\n", desc);
  printf("--------------------------------------------------\n");
}

static int test_end(int passed, const char* fmt, ...)
{
  printf("--------------------------------------------------\n");
  if (passed)
    printf(" Test passed");
  else
    printf(" Test failed");
  if (fmt)
  {
    va_list ap;
    printf(": ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
  }
  printf("\n");
  //printf("**************************************************\n");
  return passed ? 0 : 1;
}
/*
static void test_print(const char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);

    printf("  ");
    vprintf(fmt,  ap);
    printf("\n");

    va_end(ap);
}
*/
static void test_failed(const char* fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);

  printf("  [FAILED] ");
  vprintf(fmt, ap);
  printf("\n");

  va_end(ap);
}

static void test_ok(const char* fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);

  printf("  [OK] ");
  vprintf(fmt, ap);
  printf("\n");

  va_end(ap);
}

static int test_perft_correctness(const char* filename)
{
  position_t* position;
  char line[1024];
  FILE* file;
  int failed = 0;
  int ok = 0;

  test_start("Move generation correctness");

  file = fopen(filename, "r");
  if (!file)
  {
    test_failed("loading file '%s'", filename);
    test_end(0, NULL);
    return 1;
  }

  position = position_new();

  while (fgets(line, 1024, file))
  {
    char* l = strip(line);
    if (*l == '#' || *l == 0)
      continue;
    if (*l == '!')
    {
      //printf("%s\n", l+1);
      continue;
    }

    char* pos = strchr(l, ' ');
    if (!pos)
    {
      test_failed("invalid test line [%s]", l);
      continue;
    }
    *pos++ = 0;
    int depth = atoi(l);
    l = pos;

    pos = strchr(l, ' ');
    if (!pos)
    {
      test_failed("invalid test line [%s]", l);
      continue;
    }
    *pos++ = 0;
    uint64_t num = strtoll(l, NULL, 10);
    l = pos;

    if (!position_set(position, l))
    {
      test_failed("invalid fen string [%s]", l);
      failed++;
      continue;
    }

    TC.start();
    uint64_t result = perft(position, depth);
    auto ms = TC.elapsed();
    char str[100] = "";
    if (ms.count() > 0)
      sprintf(str, "(%llu/ms)", result / ms.count());

    if (result != num)
    {
      position_print(position, White);
      test_failed("depth %d: result %llu, expected %llu %s",
        depth, result, num, str);
      failed++;
    }
    else
    {
      test_ok("depth %d: result %llu %s", depth, result, str);
      ok++;
    }
  }

  fclose(file);
  position_destroy(position);

  return test_end(failed == 0, "%d failed, %d ok", failed, ok);
}

static int checkPos(const char* fen, unsigned depth)
{
  uint64_t moves;
  position_t* pos;

  TT_clear();
  pos = position_new();
  if (!position_set(pos, fen))
  {
    test_failed("invalid fen string [%s]", fen);
    return 1;
  }
  TC.start();
  moves = perft(pos, depth);
  auto ms = TC.elapsed().count();
  test_ok("depth %d: %lld moves (%.3f seconds, %llu/ms)",
    depth, moves, (double)ms / 1000.0, moves / ms);

  position_destroy(pos);
  return 0;
}

static int test_perft_performance()
{
  int failed = 0;

  test_start("Move generation performance");

  failed += checkPos("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6);
  failed += checkPos("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 5);
  failed += checkPos("8/p7/8/1P6/K1k3p1/6P1/7P/8 w - -", 9);
  failed += checkPos("r7/q3p3/2pk4/3p1Q2/3P4/pRP5/P5PP/6K1 w - -", 6);

  return test_end(failed == 0, NULL);
}

static int checkSearch(const char* fen)
{
  position_t* pos;
  Move move;

  pos = position_new();
  if (!position_set(pos, fen))
  {
    test_failed("invalid fen string [%s]", fen);
    return 1;
  }

  TT_clear();
  TC.reset();
  TC.setMoveTime(std::chrono::seconds(1));
  TC.start();

  move = search_do(pos);
  auto ms = TC.elapsed().count();

  test_ok("depth %d: %s (%.3f seconds, %d nodes, %d/ms)", Depth, move_format(move),
    (double)ms / 1000.0, stats_get(ST_NODE), stats_get(ST_NODE) / ms);

  position_destroy(pos);
  return 0;
}


static int test_search_performance()
{
  int failed = 0;

  test_start("Search performance");

  failed += checkSearch("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  failed += checkSearch("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");
  failed += checkSearch("8/p7/8/1P6/K1k3p1/6P1/7P/8 w - -");
  failed += checkSearch("r7/q3p3/2pk4/3p1Q2/3P4/pRP5/P5PP/6K1 w - -");

  return test_end(failed == 0, NULL);
}


static void info_depth(int depth)
{
  Depth = depth;
}
static void info_pv(int score, Move* pv) {}
static void info_done() {}
static void info_curmove(Move move, int num) {}
static void search_done(position_t* pos, Move move) {}

int main(int argc, char** argv)
{
  int failed = 0;
  int caseID = -1;

  bitboards_init();
  positions_init();
  tables_init();
  prnd_init();

  IF.info_depth = info_depth;
  IF.info_pv = info_pv;
  IF.info_curmove = info_curmove;
  IF.info_done = info_done;
  IF.search_done = search_done;

  if (argc > 1)
    caseID = atoi(argv[1]);

  if (caseID == -1 || caseID == 1)
    failed += test_perft_correctness("perft.table");
  if (caseID == -1 || caseID == 2)
    failed += test_perft_performance();
  if (caseID == -1 || caseID == 3)
    failed += test_search_performance();

  printf("\n");
  printf("==================================================\n");
  if (failed > 0)
    printf(" SUMMARY: %d test cases failed\n", failed);
  else
    printf(" SUMMARY: All tests ok\n");
  printf("==================================================\n");
  printf("\n");

  tables_exit();
  return 0;
}
