#include "eval.h"
#include "stats.h"
#include "bitboard.h"

static int Phase;

static const int DoubledPawnPenalty[2][PHASE_NUM][FileNum] = {
  { { 13, 20, 23, 23, 23, 23, 20, 13 },
    { 43, 48, 48, 48, 48, 48, 48, 43 }, },
  { { 13, 20, 23, 23, 23, 23, 20, 13 },
    { 43, 48, 48, 48, 48, 48, 48, 43 }, },
};

static const int IsolatedPawnPenalty[2][PHASE_NUM][FileNum] = {
  { { 37, 54, 60, 60, 60, 60, 54, 37 },
    { 45, 52, 52, 52, 52, 52, 52, 45 }, },
  { { 25, 36, 40, 40, 40, 40, 36, 25 },
    { 30, 35, 35, 35, 35, 35, 35, 30 }, },
};

static const int BackwardPawnPenalty[2][PHASE_NUM][FileNum] = {
  { { 30, 43, 49, 49, 49, 49, 43, 30 },
    { 42, 46, 46, 46, 46, 46, 46, 42 }, },
  { { 20, 29, 33, 33, 33, 33, 29, 20 },
    { 28, 31, 31, 31, 31, 31, 31, 28 }, },
};

static const int ChainPawnBonus[PHASE_NUM][FileNum] = {
  { 11, 13, 13, 14, 14, 13, 13, 11 },
  { -1, -1, -1, -1, -1, -1, -1, -1 },
};

static const int CandidatePawnBonus[PHASE_NUM][FileNum] = {
  {  0,  6,  6, 14, 34, 83, 0, 0 },
  {  0, 13, 13, 29, 68,166, 0, 0 },
};


static int eval_material(position_t* pos)
{
  int res = 0;

  if (Phase == PHASE_EG)
  {
    res = EG_VAL(pos->state->score);
  }
  else
  {
    res = MG_VAL(pos->state->score);
#if USE_PIECE_LIST
    if (pos->pCount[White][Bishop] == 2)
      res += PieceValue[Pawn][PHASE_MG] / 2;
    if (pos->pCount[Black][Bishop] == 2)
      res -= PieceValue[Pawn][PHASE_MG] / 2;
#endif
  }
  return res;
}

static int eval_mobility(position_t* pos)
{
  int res = 0;
#if USE_PIECE_LIST
  const int mBonus[ColorNum][PHASE_NUM][PieceNum] = {
      { {0, 0, +2, +5, +3, +5, 0}, {0, 0, +2, +3, +4, +5, 0}, },
      { {0, 0,-2, -5, -3, -5, 0}, {0, 0, -2, -3, -4, -5, 0}, },
  };
  bitboard_t occ = pos->color[White] | pos->color[Black];

  for (auto c = (int)White; c <= (int)Black; c++)
  {
    auto color = (Color)c;
    bitboard_t target = ~pos->color[color];
    Square* pieces;
    int temp, square;

    pieces = pos->piece[color][Knight];
    for (temp = 0, square = *pieces; *pieces != SquareNone; pieces++)
      temp += popcount(B_KnightAttacks[square] & target);
    res += temp * mBonus[color][Phase][Knight];

    pieces = pos->piece[color][Bishop];
    for (temp = 0, square = *pieces; *pieces != SquareNone; pieces++)
      temp += popcount(sliding_attack_bishop(square, occ) & target);
    res += temp * mBonus[color][Phase][Bishop];

    pieces = pos->piece[color][Rook];
    for (temp = 0, square = *pieces; *pieces != SquareNone; pieces++)
      temp += popcount(sliding_attack_rook(square, occ) & target);
    res += temp * mBonus[color][Phase][Rook];

    pieces = pos->piece[color][Queen];
    for (temp = 0, square = *pieces; *pieces != SquareNone; pieces++)
      temp += popcount((sliding_attack_bishop(square, occ) | sliding_attack_bishop(square, occ)) & target);
    res += temp * mBonus[color][Phase][Queen];
  }
#endif

  return res;
}

static int eval_pawn(position_t* pos)
{
  int res = 0;
#if USE_PIECE_LIST
  bitboard_t wPawns = pos->type[Pawn] & pos->color[White];
  bitboard_t bPawns = pos->type[Pawn] & pos->color[Black];
  bitboard_t allPawns = pos->type[Pawn];
  bitboard_t b;

  Square* pieces = pos->piece[White][Pawn];
  Square square;
  while ((square = *pieces++) != SquareNone)
  {
    int r = rank_of(square);
    int f = file_of(square);
    int cPawn = (wPawns & B_AdjacentFile[f] & (B_Rank[r] | B_Rank[r - 1])) != 0;
    int iPawn = (wPawns & B_AdjacentFile[f]) == 0;
    int dPawn = (wPawns & B_Forward[White][square]) != 0;
    int oPawn = (bPawns & B_Forward[White][square]) != 0;
    int pPawn = (bPawns & B_PassedPawnMask[White][square]) == 0;
    int bPawn = 0;
    int candidate = 0;

    if (!pPawn && !iPawn && !cPawn &&
      !(wPawns & B_AttackSpawnMask[Black][square]) &&
      !(bPawns & B_WPawnAttacks[square]))
    {
      b = B_WPawnAttacks[square];
      while (!(b & allPawns))
        b <<= 8;
      bPawn = ((b | (b << 8)) & bPawns) != 0;
    }

    candidate =
      !oPawn && !pPawn && !bPawn && !iPawn &&
      (b = B_AttackSpawnMask[Black][square + 8] & wPawns) != 0 &&
      popcount_max15(b) >= popcount_max15(B_AttackSpawnMask[White][square] & bPawns);

    if (dPawn)
      res -= DoubledPawnPenalty[oPawn][Phase][f];
    else if (pPawn)
      res += 100;

    if (iPawn)
      res -= IsolatedPawnPenalty[oPawn][Phase][f];
    if (bPawn)
      res -= BackwardPawnPenalty[oPawn][Phase][f];
    if (cPawn)
      res += ChainPawnBonus[Phase][f];
    if (candidate)
      res += CandidatePawnBonus[Phase][f];
  }

  pieces = pos->piece[Black][Pawn];
  while ((square = *pieces++) != SquareNone)
  {
    int r = rank_of(square);
    int f = file_of(square);
    int cPawn = (bPawns & B_AdjacentFile[f] & (B_Rank[r] | B_Rank[r + 1])) != 0;
    int iPawn = (bPawns & B_AdjacentFile[f]) == 0;
    int dPawn = (bPawns & B_Forward[Black][square]) != 0;
    int oPawn = (wPawns & B_Forward[Black][square]) != 0;
    int pPawn = (wPawns & B_PassedPawnMask[Black][square]) == 0;
    int bPawn = 0;
    int candidate = 0;

    if (!pPawn && !iPawn && !cPawn &&
      !(bPawns & B_AttackSpawnMask[White][square]) &&
      !(wPawns & B_BPawnAttacks[square]))
    {
      b = B_WPawnAttacks[square];
      while (!(b & allPawns))
        b >>= 8;
      bPawn = ((b | (b >> 8)) & wPawns) != 0;
    }

    candidate =
      !oPawn && !pPawn && !bPawn && !iPawn &&
      (b = B_AttackSpawnMask[White][square + 8] & bPawns) != 0 &&
      popcount_max15(b) >= popcount_max15(B_AttackSpawnMask[Black][square] & wPawns);

    if (dPawn)
      res += DoubledPawnPenalty[oPawn][Phase][f];
    else if (pPawn)
      res -= 100;

    if (iPawn)
      res += IsolatedPawnPenalty[oPawn][Phase][f];
    if (bPawn)
      res += BackwardPawnPenalty[oPawn][Phase][f];
    if (cPawn)
      res -= ChainPawnBonus[Phase][f];
    if (candidate)
      res -= CandidatePawnBonus[Phase][f];
  }
#endif

  return res;
}

int eval_static(position_t* pos)
{
  int res = 0;

  stats_inc(ST_EVAL, 1);

  if (pos->state->npMaterial[White] < PieceValue[PieceNone][PHASE_MG] &&
    pos->state->npMaterial[Black] < PieceValue[PieceNone][PHASE_MG])
    Phase = PHASE_EG;
  else
    Phase = PHASE_MG;

  res += eval_material(pos);
  res += eval_mobility(pos);
  res += eval_pawn(pos);

  if (pos->to_move == White)
    return res;
  else
    return -res;
}


