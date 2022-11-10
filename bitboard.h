#ifndef BITBOARD_H__
#define BITBOARD_H__

#include "types.h"
#include <stdio.h>
#ifdef _WIN32
#  include <intrin.h>
#endif

//#define BB(square) (1ULL << (square))
//#define BB_RANK(square) (B_Rank1 << ((square) & ~7))
//#define BB_FILE(square) (B_FileA << ((square) & 7))
#define BB(square) (B_Square[square])
#define BB_RANK(square) (B_Rank1 << ((square) & ~7))
#define BB_FILE(square) (B_FileA << ((square) & 7))

#define BC_MORE_THAN_ONE(bb) ((bb) && ((bb) & ((bb) - 1)))
#define BC_EXACTLY_ONE(bb) ((bb) && ((bb) & ((bb) - 1)) == 0)

extern const bitboard_t B_Rank1;
extern const bitboard_t B_Rank2;
extern const bitboard_t B_Rank3;
extern const bitboard_t B_Rank4;
extern const bitboard_t B_Rank5;
extern const bitboard_t B_Rank6;
extern const bitboard_t B_Rank7;
extern const bitboard_t B_Rank8;

extern const bitboard_t B_FileA;
extern const bitboard_t B_FileB;
extern const bitboard_t B_FileC;
extern const bitboard_t B_FileD;
extern const bitboard_t B_FileE;
extern const bitboard_t B_FileF;
extern const bitboard_t B_FileG;
extern const bitboard_t B_FileH;

extern const bitboard_t B_Universe;
extern const bitboard_t B_Empty;
extern const bitboard_t B_NotFileA;
extern const bitboard_t B_NotFileH;
extern const bitboard_t B_NotRankpn1;
extern const bitboard_t B_NotRank8;

extern bitboard_t B_Square[SQ_NUM];
extern bitboard_t B_File[FileNum];
extern bitboard_t B_Rank[RankNum];

extern bitboard_t B_KnightAttacks[SQ_NUM];
extern bitboard_t B_KingAttacks[SQ_NUM];
extern bitboard_t B_WPawnAttacks[SQ_NUM];
extern bitboard_t B_BPawnAttacks[SQ_NUM];
extern bitboard_t B_BishopAttacks[SQ_NUM];
extern bitboard_t B_RookAttacks[SQ_NUM];
extern bitboard_t B_QueenAttacks[SQ_NUM];

extern bitboard_t B_Between[SQ_NUM][SQ_NUM];
extern bitboard_t B_InFront[C_NUM][RankNum];
extern bitboard_t B_Forward[C_NUM][SQ_NUM];
extern bitboard_t B_AdjacentFile[FileNum];
extern bitboard_t B_PassedPawnMask[C_NUM][SQ_NUM];
extern bitboard_t B_AttackSpawnMask[C_NUM][SQ_NUM];

void bitboards_init(void);

bitboard_t sliding_attack_rook(int square, bitboard_t occupied);
bitboard_t sliding_attack_bishop(int square, bitboard_t occupied);
extern int squares_aligned(int s1, int s2, int s3);

void bitboard_print(FILE* fd, bitboard_t bb);

int popcount_max15(bitboard_t b);
int popcount(bitboard_t b);

//return BSFTable[bsf_index(b)];
#ifdef _WIN32
inline int lsb(bitboard_t b) {
  unsigned long idx;

  if (b & 0xffffffff) {
    _BitScanForward(&idx, (int32_t)b);
    return (int)(idx);
  }
  else {
    _BitScanForward(&idx, (int32_t)(b >> 32));
    return (int)(idx + 32);
  }
}
#endif

#ifdef _WIN32
#  define get_lsb(b) lsb(b)
#elif IS_64BIT
#  define get_lsb(b) __builtin_ctzl(b)
#else
#  define get_lsb(b) ((int)(b) ? __builtin_ctz((int)(b)) : 32 + __builtin_ctz((int)((b) >> 32)))
#endif


//extern int pop_lsb(bitboard_t* b);
//extern int get_lsb(bitboard_t b);


#define PLSB(to, b) to = get_lsb(b); b &= (b - 1)
#define GLSB(b) get_lsb(b)



#endif
