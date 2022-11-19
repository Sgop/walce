#ifndef TYPES_H__
#define TYPES_H__

#include "platform.h"
#include "debug.h"

typedef uint64_t bitboard_t;
typedef uint64_t pkey_t;

enum Color {
  White,
  Black,
  ColorNone,  //FIXME: get rid of this
  ColorNum = 2
};

constexpr Color operator~(Color c) {
  return Color(c ^ Black);
}

enum PieceType {
  PieceNone = 0,
  Pawn,
  Knight,
  Bishop,
  Rook,
  Queen,
  King,
  PieceNum = 7
};

// move stuff
enum MoveType {
  NormalMove = 0,
  Promotion = (1 << 14),
  EnPassant = (2 << 14),
  Castle = (3 << 14),
};

// 6 bit from, 6 bit to, 2 bit promotion piece, 2 bit move type
enum Move : int {
  MOVE_NONE = 0,
};

constexpr Move make_move(int from, int to) {
  return Move((from << 6) | to);
}

template<MoveType T>
constexpr Move make(int from, int to, int prom = Knight) {
  return Move(T | ((prom - Knight) << 12) | (from << 6) | to);
}

constexpr MoveType type_of(Move m) {
  return MoveType(m & (3 << 14));
}

constexpr PieceType promotionPiece(Move move) {
  return PieceType(((move >> 12) & 3) + Knight);
}

enum Square : int {
  A1, B1, C1, D1, E1, F1, G1, H1,
  A2, B2, C2, D2, E2, F2, G2, H2,
  A3, B3, C3, D3, E3, F3, G3, H3,
  A4, B4, C4, D4, E4, F4, G4, H4,
  A5, B5, C5, D5, E5, F5, G5, H5,
  A6, B6, C6, D6, E6, F6, G6, H6,
  A7, B7, C7, D7, E7, F7, G7, H7,
  A8, B8, C8, D8, E8, F8, G8, H8, SquareNum,
  SquareNone = SquareNum,

  //SQUARE_ZERO = 0,
  //SQUARE_NB = 64
};

constexpr Square from_sq(Move m) {
  return Square((m >> 6) & 0x3F);
}

constexpr Square to_sq(Move m) {
  return Square(m & 0x3F);
}
enum File : int {
  FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH, FileNum,
};

enum Rank : int {
  Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8, RankNum,
};

constexpr File file_of(Square s) {
  return File(s & 7);
}

constexpr Rank rank_of(Square s) {
  return Rank(s >> 3);
}

constexpr Square make_square(File f, Rank r) {
  return Square((r << 3) + f);
}

enum Direction : int {
  DELTA_NONE = 0,
  DELTA_N = 8,
  DELTA_E = 1,
  DELTA_S = -DELTA_N,
  DELTA_W = -DELTA_E,
  DELTA_NN = DELTA_N + DELTA_N,
  DELTA_NE = DELTA_N + DELTA_E,
  DELTA_SE = DELTA_S + DELTA_E,
  DELTA_SS = DELTA_S + DELTA_S,
  DELTA_SW = DELTA_S + DELTA_W,
  DELTA_NW = DELTA_N + DELTA_W
};

enum {
  MAX_MOVES = 256,
  MAX_PLY = 100,
  MAX_PLY_PLUS_2 = MAX_PLY + 2,
};

enum {
  V_DRAW = 0,
  V_WIN = 15000,
  V_MATE = 30000,
  V_INF = 30001,
  V_NONE = 30002,
  V_MATE_PLY = V_MATE - MAX_PLY,
};

#define V_MATE_IN(ply) (V_MATE - (ply))

enum CastleRights {
  CASTLING_NONE = 0,
  WHITE_OO = 1,
  WHITE_OOO = WHITE_OO << 1,
  BLACK_OO = WHITE_OO << 2,
  BLACK_OOO = WHITE_OO << 3,
  CASTLING_ALL = 0xf,
};

enum {
  B_NONE = 0,
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
  PHASE_MG,
  PHASE_EG,
  PHASE_NUM,
};


#define ENABLE_BASE_OPERATORS_ON(T)                                \
constexpr T operator+(T d1, int d2) { return T(int(d1) + d2); }    \
constexpr T operator-(T d1, int d2) { return T(int(d1) - d2); }    \
constexpr T operator-(T d) { return T(-int(d)); }                  \
inline T& operator+=(T& d1, int d2) { return d1 = d1 + d2; }       \
inline T& operator-=(T& d1, int d2) { return d1 = d1 - d2; }

#define ENABLE_INCR_OPERATORS_ON(T)                                \
inline T& operator++(T& d) { return d = T(int(d) + 1); }           \
inline T& operator--(T& d) { return d = T(int(d) - 1); }

#define ENABLE_FULL_OPERATORS_ON(T)                                \
ENABLE_BASE_OPERATORS_ON(T)                                        \
constexpr T operator*(int i, T d) { return T(i * int(d)); }        \
constexpr T operator*(T d, int i) { return T(int(d) * i); }        \
constexpr T operator/(T d, int i) { return T(int(d) / i); }        \
constexpr int operator/(T d1, T d2) { return int(d1) / int(d2); }  \
inline T& operator*=(T& d, int i) { return d = T(int(d) * i); }    \
inline T& operator/=(T& d, int i) { return d = T(int(d) / i); }

//ENABLE_FULL_OPERATORS_ON(Value)
ENABLE_FULL_OPERATORS_ON(Direction)

//ENABLE_INCR_OPERATORS_ON(Piece)
ENABLE_INCR_OPERATORS_ON(PieceType)
ENABLE_INCR_OPERATORS_ON(Square)
ENABLE_INCR_OPERATORS_ON(File)
ENABLE_INCR_OPERATORS_ON(Rank)
ENABLE_INCR_OPERATORS_ON(Color)
ENABLE_BASE_OPERATORS_ON(File);
ENABLE_BASE_OPERATORS_ON(Rank);

#undef ENABLE_FULL_OPERATORS_ON
#undef ENABLE_INCR_OPERATORS_ON
#undef ENABLE_BASE_OPERATORS_ON

constexpr Square operator+(Square s, Direction d) { return Square(int(s) + int(d)); }
constexpr Square operator-(Square s, Direction d) { return Square(int(s) - int(d)); }
inline Square& operator+=(Square& s, Direction d) { return s = s + d; }
inline Square& operator-=(Square& s, Direction d) { return s = s - d; }


#endif

