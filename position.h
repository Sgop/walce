#ifndef BOARD_H__
#define BOARD_H__

#include "types.h"
#include "bitboard.h"
#include <stdio.h>

// piece values
extern const char P_CHAR[PieceNum];
extern const int PieceValue[PieceNum][PHASE_NUM];
extern int SquareValue[PieceNum][ColorNum][SquareNum];
extern int CastleMaskFrom[SquareNum];
extern int CastleMaskTo[SquareNum];

#define MAKE_SCORE(mg, eg) (((mg) << 16) + (eg))
#define MG_VAL(s) ((((s) + 32768) & ~0xffff) / 0x10000)
#define EG_VAL(s) ((int)((unsigned)(s) & 0x7fffu) - (int)((unsigned)(s) & 0x8000u))

// position things
typedef struct _state_t state_t;
struct _state_t {
    Square enPassant;        // en-passant square
    int rule50;              // number of moves since last capture or pawn move
    int plyNum;              // depth in search tree, starting from 0
    int castleRights;        // castle rights
    state_t* prev;

    Move move;             // move done
    PieceType captured;            // captured piece by move
    int npMaterial[ColorNum];   // non-pawn material
    int score;               // accumulated piece value (white point of view)
    pkey_t key;                 // 

    bitboard_t checkers;     // checkers pieces (opponent)
    bitboard_t pinned;       // pinned pieces (side to move)
    bitboard_t hCheckers;
};


#define USE_PIECE_LIST 1

typedef struct {
    bitboard_t color[ColorNum];
    bitboard_t type[PieceNum];
    PieceType board[SquareNum];
#if USE_PIECE_LIST    
    int pCount[ColorNum][PieceNum];
    Square piece[ColorNum][PieceNum][16];
    int index[SquareNum];
#endif
    Color to_move;
    state_t* state;
    Move moves[1000];
    state_t states[1000];
} position_t;


void positions_init();

char* move_format(Move move);

position_t* position_new(void);
void position_reset(position_t* pos);
void position_destroy(position_t* pos);
void position_print(position_t* pos, Color color);
int position_set(position_t* pos, const char* fen);
char* position_get(position_t* pos);
void position_move(position_t* pos, Move move);
int position_unmove(position_t* pos);


// some functions to get status
int position_king_square(position_t* pos, Color color);
bitboard_t position_rook_pieces(position_t* pos, Color color);
bitboard_t position_bishop_pieces(position_t* pos, Color color);
bitboard_t position_pinned(position_t* pos);
bitboard_t position_hidden_checkers(position_t* pos);
bitboard_t position_attacks_from(position_t* pos, int square, int piece, bitboard_t occ);
bitboard_t position_attackers_to(position_t* pos, int square);
bitboard_t position_attackers_to_ext(position_t* pos, int square, bitboard_t occ);
bitboard_t position_checkers(position_t* pos);


void position_get_checkinfo(position_t* pos);
int position_move_gives_check(position_t* pos, Move move);
int position_in_check(position_t* pos);
int position_is_draw(position_t* pos, int checkRepetition, int check3Fold);
int position_pseudo_is_legal(position_t* pos, Move move);
int position_move_is_pseudo(position_t* pos, Move move);
int position_move_is_legal(position_t* pos, Move move);
int position_see_sign(position_t* pos, Move move);
int position_see(position_t* pos, Move move);
int position_is_cap_or_prom(position_t* pos, Move move);
int position_is_cap(position_t* pos, Move move);
int position_pawns_on7(position_t* pos, Color stm);

void position_check_vars(position_t* pos);

#endif
