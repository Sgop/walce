#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bitboard.h"
#include "prnd.h"


const bitboard_t B_Rank1 = 0xffULL << A1;
const bitboard_t B_Rank2 = 0xffULL << A2;
const bitboard_t B_Rank3 = 0xffULL << A3;
const bitboard_t B_Rank4 = 0xffULL << A4;
const bitboard_t B_Rank5 = 0xffULL << A5;
const bitboard_t B_Rank6 = 0xffULL << A6;
const bitboard_t B_Rank7 = 0xffULL << A7;
const bitboard_t B_Rank8 = 0xffULL << A8;

const bitboard_t B_FileA = 0x0101010101010101ULL << A1;
const bitboard_t B_FileB = 0x0101010101010101ULL << B1;
const bitboard_t B_FileC = 0x0101010101010101ULL << C1;
const bitboard_t B_FileD = 0x0101010101010101ULL << D1;
const bitboard_t B_FileE = 0x0101010101010101ULL << E1;
const bitboard_t B_FileF = 0x0101010101010101ULL << F1;
const bitboard_t B_FileG = 0x0101010101010101ULL << G1;
const bitboard_t B_FileH = 0x0101010101010101ULL << H1;

const bitboard_t B_Universe = 0xffffffffffffffffULL;
const bitboard_t B_Empty = 0x0000000000000000ULL;
const bitboard_t B_NotFileA = 0xfefefefefefefefeULL;
const bitboard_t B_NotFileH = 0x7f7f7f7f7f7f7f7fULL;
const bitboard_t B_NotRank1 = 0xffffffffffffff00ULL;
const bitboard_t B_NotRank8 = 0x00ffffffffffffffULL;

bitboard_t B_Square[SquareNum];
bitboard_t B_File[FileNum];
bitboard_t B_Rank[RankNum];

static const int RDeltas[] = { DELTA_N,  DELTA_E,  DELTA_S,  DELTA_W };
static const int BDeltas[] = { DELTA_NE, DELTA_SE, DELTA_SW, DELTA_NW };

static int BSFTable[SquareNum];

// step attacks
bitboard_t B_KnightAttacks[SquareNum] = { 0 };
bitboard_t B_KingAttacks[SquareNum] = { 0 };
bitboard_t B_WPawnAttacks[SquareNum] = { 0 };
bitboard_t B_BPawnAttacks[SquareNum] = { 0 };
// pseudo sliding attacks
bitboard_t B_BishopAttacks[SquareNum] = { 0 };
bitboard_t B_RookAttacks[SquareNum] = { 0 };
bitboard_t B_QueenAttacks[SquareNum] = { 0 };
// squares between two other squares
bitboard_t B_Between[SquareNum][SquareNum];
bitboard_t B_InFront[ColorNum][8];
bitboard_t B_Forward[ColorNum][SquareNum];
bitboard_t B_AdjacentFile[FileNum];
bitboard_t B_PassedPawnMask[ColorNum][SquareNum];
bitboard_t B_AttackSpawnMask[ColorNum][SquareNum];
// sliding attacks
static bitboard_t RTable[0x19000];
static bitboard_t BTable[0x19000];
static bitboard_t RMagics[SquareNum];
static bitboard_t BMagics[SquareNum];
static bitboard_t BMasks[SquareNum];
static bitboard_t RMasks[SquareNum];
static unsigned RShifts[SquareNum];
static unsigned BShifts[SquareNum];
bitboard_t* RAttacks[SquareNum];
bitboard_t* BAttacks[SquareNum];

typedef unsigned (IndexFunc)(int, bitboard_t);


static int CMASK[16] =
{
    0, 1, 1, 2,
    1, 2, 2, 3,
    1, 2, 2, 3,
    2, 3, 3, 4,
};

int popcount(bitboard_t b)
{
#if 1
  b -= (b >> 1) & 0x5555555555555555ULL;
  b = ((b >> 2) & 0x3333333333333333ULL) + (b & 0x3333333333333333ULL);
  b = ((b >> 4) + b) & 0x0F0F0F0F0F0F0F0FULL;
  return (b * 0x0101010101010101ULL) >> 56;
#else
  int res = 0;
  while (b)
  {
    res += CMASK[b & 0xf];
    b >>= 4;
  }
  return res;
#endif
}

int popcount_max15(bitboard_t b)
{
  b -= (b >> 1) & 0x5555555555555555ULL;
  b = ((b >> 2) & 0x3333333333333333ULL) + (b & 0x3333333333333333ULL);
  return (b * 0x1111111111111111ULL) >> 60;
}




static unsigned bsf_index(bitboard_t b)
{
  b ^= (b - 1);
#ifdef IS_64BIT
  /* bitscan by Martin Laeuter (1997) improved by Kim Walisch (2012) */
  return (b * 0x03f79d71b4cb0a89ULL) >> 58;
#else
  /* 32-bit version by Matt Taylor (2003) */
  unsigned folded = (int)b ^ (b >> 32);
  return (folded * 0x78291acf) >> 26;
#endif
}

static int square_distance(Square s1, Square s2)
{
  int d1 = abs(file_of(s1) - file_of(s2));
  int d2 = abs(rank_of(s1) - rank_of(s2));
  return d2 > d1 ? d2 : d1;
}

static int is_ok(int square)
{
  return (square >= A1 && square <= H8);
}

static bitboard_t sliding_attack(const int deltas[], Square square, bitboard_t occupied)
{
  bitboard_t attack = 0;
  int i1, s;

  for (i1 = 0; i1 < 4; i1++)
  {
    for (s = square + deltas[i1];
      is_ok(s) && square_distance(Square(s), Square(s - deltas[i1])) == 1;
      s += deltas[i1])
    {
      attack |= BB(s);
      if (occupied & BB(s))
        break;
    }
  }
  return attack;
}

static unsigned magic_index_rook(int s, bitboard_t occ)
{
#ifdef IS_64BIT
  return (unsigned)(((occ & RMasks[s]) * RMagics[s]) >> RShifts[s]);
#else
  unsigned lo = (unsigned)occ & (unsigned)RMasks[s];
  unsigned hi = (unsigned)(occ >> 32) & (unsigned)(RMasks[s] >> 32);
  return (lo * (unsigned)RMagics[s] ^ hi * (unsigned)(RMagics[s] >> 32)) >> RShifts[s];
#endif
}

static unsigned magic_index_bishop(int s, bitboard_t occ)
{
#ifdef IS_64BIT
  return (unsigned)(((occ & BMasks[s]) * BMagics[s]) >> BShifts[s]);
#else
  unsigned lo = (unsigned)occ & (unsigned)BMasks[s];
  unsigned hi = (unsigned)(occ >> 32) & (unsigned)(BMasks[s] >> 32);
  return (lo * (unsigned)BMagics[s] ^ hi * (unsigned)(BMagics[s] >> 32)) >> BShifts[s];
#endif
}

static bitboard_t pick_random(int booster)
{
  int s1 = booster & 63;
  int s2 = (booster >> 6) & 63;
  bitboard_t m = prand64();

  m = (m >> s1) | (m << (64 - s1));
  m &= prand64();
  m = (m >> s2) | (m << (64 - s2));
  return m & prand64();
}

static void init_magics(
  bitboard_t table[], bitboard_t* attacks[], bitboard_t magics[],
  bitboard_t masks[], unsigned shifts[], const int deltas[], IndexFunc func)
{
#ifdef IS_64BIT
  int MagicBoosters[8] = { 1059, 3608,  605, 3234, 3326,   38, 2029, 3043 };
#else
  int MagicBoosters[8] = { 3191, 2184, 1310, 3618, 2091, 1308, 2452, 3996 };
#endif
  bitboard_t occupancy[4096], reference[4096], b;
  int i, size, booster;

  prnd_init();

  // attacks[s] is a pointer to the beginning of the attacks table for square 's'
  attacks[A1] = table;

  for (Square s = A1; s <= H8; ++s)
  {
    // Board edges are not considered in the relevant occupancies
    bitboard_t edges =
      ((B_Rank1 | B_Rank8) & ~BB_RANK(s)) | ((B_FileA | B_FileH) & ~BB_FILE(s));

    // Given a square 's', the mask is the bitboard of sliding attacks from
    // 's' computed on an empty board. The index must be big enough to contain
    // all the attacks for each possible subset of the mask and so is 2 power
    // the number of 1s of the mask. Hence we deduce the size of the shift to
    // apply to the 64 or 32 bits word to get the index.
    masks[s] = sliding_attack(deltas, s, 0) & ~edges;
#ifdef IS_64BIT
    shifts[s] = 64 - popcount_max15(masks[s]);
#else
    shifts[s] = 32 - popcount_max15(masks[s]);
#endif
    // Use Carry-Rippler trick to enumerate all subsets of masks[s] and
    // store the corresponding sliding attack bitboard in reference[].
    b = size = 0;
    do {
      occupancy[size] = b;
      reference[size++] = sliding_attack(deltas, s, b);
      b = (b - masks[s]) & masks[s];
    } while (b);

    // Set the offset for the table of the next square. We have individual
    // table sizes for each square with "Fancy Magic Bitboards".
    if (s < H8)
      attacks[s + 1] = attacks[s] + size;

    booster = MagicBoosters[rank_of(s)];

    // Find a magic for square 's' picking up an (almost) random number
    // until we find the one that passes the verification test.
    do {
      int cnt = 0;
      do {
        magics[s] = pick_random(booster);
        if (++cnt > 1000)
          exit(0);
      } while (popcount_max15((magics[s] * masks[s]) >> 56) < 6);

      //printf("geschafft %d\n", cnt);
      memset(attacks[s], 0, size * sizeof(bitboard_t));

      // A good magic must map every possible occupancy to an index that
      // looks up the correct sliding attack in the attacks[s] database.
      // Note that we build up the database for square 's' as a side
      // effect of verifying the magic.
      for (i = 0; i < size; i++)
      {
        bitboard_t* attack = &attacks[s][func(s, occupancy[i])];

        if (*attack && *attack != reference[i])
          break;

        ASSERT_IF_FAIL(reference[i] != 0, "problem");
        *attack = reference[i];
      }
    } while (i != size);
  }
}

void bitboards_init(void)
{
  int r, f;

  { // bitboards
    for (auto i1 = A1; i1 <= H8; ++i1)
      B_Square[i1] = 1ULL << i1;

    for (auto i1 = 0; i1 < 8; ++i1)
    {
      B_File[i1] = B_FileA << i1;
      B_Rank[i1] = B_Rank1 << (i1 * 8);
    }
  }

  { // create table used to find index of lsb
    for (auto i1 = 0; i1 < H8; ++i1)
      BSFTable[bsf_index(1ULL << i1)] = i1;
  }

  { // create attack tables used to generate step attack moves
    Direction stepWPawn[] = { DELTA_NE, DELTA_NW, DELTA_NONE };
    Direction stepBPawn[] = { DELTA_SE, DELTA_SW, DELTA_NONE };
    Direction stepKnight[] = { 
      DELTA_N + DELTA_NE, DELTA_N + DELTA_NW,
      DELTA_E + DELTA_NE, DELTA_E + DELTA_SE,
      DELTA_W + DELTA_NW, DELTA_W + DELTA_SW,
      DELTA_S + DELTA_SE, DELTA_S + DELTA_SW, DELTA_NONE };
    Direction stepKing[] = {
      DELTA_NE, DELTA_NW, DELTA_SE, DELTA_SW,
      DELTA_N, DELTA_W, DELTA_E, DELTA_S, DELTA_NONE
    };

#define VALID_STEP(from, to)  (is_ok(to) && square_distance(from, to) < 3)

    for (auto i1 = A1; i1 <= H8; ++i1)
    {
      for (int s = 0; stepWPawn[s] != DELTA_NONE; s++)
      {
        Square to = i1 + stepWPawn[s];
        if (VALID_STEP(i1, to))
          B_WPawnAttacks[i1] |= BB(to);
      }
      for (int s = 0; stepBPawn[s] != DELTA_NONE; ++s)
      {
        Square to = i1 + stepBPawn[s];
        if (VALID_STEP(i1, to))
          B_BPawnAttacks[i1] |= BB(to);
      }
      for (int s = 0; stepKing[s] != DELTA_NONE; ++s)
      {
        Square to = i1 + stepKing[s];
        if (VALID_STEP(i1, to))
          B_KingAttacks[i1] |= BB(to);
      }
      for (int s = 0; stepKnight[s] != DELTA_NONE; ++s)
      {
        Square to = i1 + stepKnight[s];
        if (VALID_STEP(i1, to))
          B_KnightAttacks[i1] |= BB(to);
      }
    }
#undef VALID_STEP
  }

  { // create attack tables used to generate sliding attack moves
    init_magics(RTable, RAttacks, RMagics, RMasks, RShifts, RDeltas, magic_index_rook);
    init_magics(BTable, BAttacks, BMagics, BMasks, BShifts, BDeltas, magic_index_bishop);
  }

  { // create pseudo sliding tables
    for (auto i1 = A1; i1 <= H8; ++i1)
    {
      B_BishopAttacks[i1] = sliding_attack_bishop(i1, 0);
      B_RookAttacks[i1] = sliding_attack_rook(i1, 0);
      B_QueenAttacks[i1] = B_RookAttacks[i1] | B_BishopAttacks[i1];
    }
  }

  { // create other util bitboards
    for (r = Rank1; r <= Rank8; ++r)
    {
      B_InFront[White][r] = r == Rank8 ? B_Empty : (B_Universe << ((r + 1) * 8));
      B_InFront[Black][r] = r == Rank1 ? B_Empty : (B_Universe >> (64 - r * 8));
    }
    for (f = FileA; f <= FileH; ++f)
    {
      B_AdjacentFile[f] = (f > FileA ? B_File[f - 1] : 0) | (f < FileH ? B_File[f + 1] : 0);
    }
    for (auto c = White; c <= Black; ++c)
    {
      for (auto i1 = A1; i1 <= H8; ++i1)
      {
        r = rank_of(i1);
        f = file_of(i1);
        B_Forward[c][i1] = B_InFront[c][r] & B_File[f];
        B_PassedPawnMask[c][i1] = B_InFront[c][r] & (B_File[f] | B_AdjacentFile[f]);
        B_AttackSpawnMask[c][i1] = B_InFront[c][r] & B_AdjacentFile[f];
      }
    }
    for (auto i1 = A1; i1 <= H8; ++i1)
    {
      for (auto i2 = A1; i2 <= H8; ++i2)
      {
        if (B_QueenAttacks[i1] & BB(i2))
        {
          int delta = (i2 - i1) / square_distance(i1, i2);
          int s;
          for (s = i1 + delta; s != i2; s += delta)
            B_Between[i1][i2] |= BB(s);
        }
      }
    }
  }
}



void bitboard_print(FILE* fd, bitboard_t bb)
{
  int i1, i2;
  for (i1 = Rank8; i1 >= Rank1; i1--)
  {
    for (i2 = FileA; i2 <= FileH; i2++)
    {
      if (bb & (1ULL << ((i1 * 8) + i2)))
        fprintf(fd, " X");
      else
        fprintf(fd, " -");
    }
    fprintf(fd, "\n");
  }
  fprintf(fd, "\n");
}
/*
int pop_lsb(bitboard_t* b)
{
    bitboard_t bb = *b;
    *b = bb & (bb - 1);
    return BSFTable[bsf_index(bb)];
}
*/
/*
int get_lsb(bitboard_t b)
{
    //return BSFTable[bsf_index(b)];
#ifdef IS_64BIT
    return __builtin_ctzl(b);
#else
    return (int)(b) ? __builtin_ctz((int)(b)) : 32 + __builtin_ctz((int)(b >> 32));
#endif
}

int pop_lsb(bitboard_t* b)
{
    const int s = get_lsb(*b);
    *b &= *b - 1;
    return s;
}
*/
int squares_aligned(int s1, int s2, int s3)
{
  return (((B_Between[s1][s2] | B_Between[s1][s3] | B_Between[s2][s3]) &
    (BB(s1) | BB(s2) | BB(s3))) != 0);
}

bitboard_t sliding_attack_rook(int square, bitboard_t occupied)
{
  //return sliding_attack(RDeltas, square, occupied);
  return RAttacks[square][magic_index_rook(square, occupied)];
}

bitboard_t sliding_attack_bishop(int square, bitboard_t occupied)
{
  //return sliding_attack(BDeltas, square, occupied);
  return BAttacks[square][magic_index_bishop(square, occupied)];
}
