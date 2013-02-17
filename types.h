#ifndef TYPES_H__
#define TYPES_H__

#include "platform.h"
#include "debug.h"

typedef uint64_t bitboard_t;
typedef uint64_t pkey_t;
// 6 bit from, 6 bit to, 2 bit type
typedef uint16_t move_t;



#define FILE_OF(s) (s & 7)
#define RANK_OF(s) (s >> 3)
#define SQ(file, rank) ((file) + ((rank) << 3))

enum {
  A1, B1, C1, D1, E1, F1, G1, H1,
  A2, B2, C2, D2, E2, F2, G2, H2,
  A3, B3, C3, D3, E3, F3, G3, H3,
  A4, B4, C4, D4, E4, F4, G4, H4,
  A5, B5, C5, D5, E5, F5, G5, H5,
  A6, B6, C6, D6, E6, F6, G6, H6,
  A7, B7, C7, D7, E7, F7, G7, H7,
  A8, B8, C8, D8, E8, F8, G8, H8, SQ_NUM,
  SQ_NONE = SQ_NUM,
  DELTA_N = 8,
  DELTA_S = -8,
  DELTA_E = 1,
  DELTA_W = -1,
  DELTA_NN = DELTA_N + DELTA_N,
  DELTA_NE = DELTA_N + DELTA_E,
  DELTA_SE = DELTA_S + DELTA_E,
  DELTA_SS = DELTA_S + DELTA_S,
  DELTA_SW = DELTA_S + DELTA_W,
  DELTA_NW = DELTA_N + DELTA_W
};

enum {
    Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8, RankNum,
};

enum {
    FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH, FileNum,
};

enum {
    MAX_MOVES = 256,
    MAX_PLY = 100,
    MAX_PLY_PLUS_2 = MAX_PLY + 2,
};

enum {
    V_DRAW = 0,
    V_WIN  = 15000,
    V_MATE = 30000,
    V_INF  = 30001,
    V_NONE = 30002,
    V_MATE_PLY = V_MATE - MAX_PLY,
};

#define V_MATE_IN(ply) (V_MATE - (ply))

typedef enum {
    C_WHITE,
    C_BLACK,
    C_NONE,
    C_NUM = 2
} color_t;

enum {
    P_NONE = 0,
    P_PAWN,
    P_KNIGHT,
    P_BISHOP,
    P_ROOK,
    P_QUEEN,
    P_KING,
    P_NUM = 7
};

enum {
    CASTLE_NONE      = 0x0,
    CASTLE_WHITE_OO  = 0x1,
    CASTLE_WHITE_OOO = 0x2,
    CASTLE_BLACK_OO  = 0x4,
    CASTLE_BLACK_OOO = 0x8,
    CASTLE_ALL       = 0xf,
};

enum {
    B_NONE  = 0,
    B_UPPER = 1,
    B_LOWER = 2,
    B_EXACT = B_UPPER | B_LOWER,
};

enum {
    D_ZERO = 0,
    D_QS_CHECKS = -1,
    D_QS_NO_CHECKS = -2,
    D_NONE = -128,
};

enum {
    MOVE_NONE = 0,
};

#endif

