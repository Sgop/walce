#ifndef BOARD_H__
#define BOARD_H__

#include "types.h"
#include "bitboard.h"
#include <stdio.h>

// piece values
extern const char P_CHAR[P_NUM];
extern const int PieceValue[P_NUM][PHASE_NUM];
extern int SquareValue[P_NUM][C_NUM][SQ_NUM];
extern int CastleMaskFrom[SQ_NUM];
extern int CastleMaskTo[SQ_NUM];

#define MAKE_SCORE(mg, eg) (((mg) << 16) + (eg))
#define MG_VAL(s) ((((s) + 32768) & ~0xffff) / 0x10000)
#define EG_VAL(s) ((int)((unsigned)(s) & 0x7fffu) - (int)((unsigned)(s) & 0x8000u))

// move stuff
typedef enum {
    M_NORMAL = 0,
    M_PROMOTION = (1 << 14),
    M_ENPASSANT = (2 << 14),
    M_CASTLE = (3 << 14),
    M_MASK = (3 << 14),
} move_type_t;

#define MAKE_MOVE(from, to) \
    ((to) | ((from) << 6))   

#define MAKE_MOVE_TYPE(from, to, type, piece) \
    ((to) | ((from) << 6) | ((piece - P_KNIGHT) << 12) | (type))

#define MOVE_TO(m)       ((m) & 0x3f)
#define MOVE_FROM(m)     (((m) >> 6) & 0x3f)
#define MOVE_TYPE(m)     ((m) & (3 << 14))
#define MOVE_PROM(m)     ((((m) >> 12) & 3) + P_KNIGHT)

// position things
typedef struct _state_t state_t;
struct _state_t {
    int enPassant;           // en-passant square
    int rule50;              // number of moves since last capture or pawn move
    int plyNum;              // depth in search tree, starting from 0
    int castleRights;        // castel rights
    state_t* prev;

    move_t move;             // move done
    int captured;            // captured piece by move
    int npMaterial[C_NUM];   // non-pawn material
    int score;               // accumulated piece value (white point of view)
    pkey_t key;                 // 

    bitboard_t checkers;     // checkers pieces (opponent)
    bitboard_t pinned;       // pinned pieces (side to move)
    bitboard_t hCheckers;
};


#define USE_PIECE_LIST 1

typedef struct {
    bitboard_t color[C_NUM];
    bitboard_t type[P_NUM];
    int board[SQ_NUM];
#if USE_PIECE_LIST    
    int pCount[C_NUM][P_NUM];
    int piece[C_NUM][P_NUM][16];
    int index[SQ_NUM];
#endif
    color_t to_move;
    state_t* state;
    move_t moves[1000];
    state_t states[1000];
} position_t;


void positions_init();

char* move_format(move_t move);

position_t* position_new(void);
void position_reset(position_t* pos);
void position_destroy(position_t* pos);
void position_print(position_t* pos);
int position_set(position_t* pos, const char* fen);
char* position_get(position_t* pos);
void position_move(position_t* pos, move_t move);
int position_unmove(position_t* pos);


// some functions to get status
int position_king_square(position_t* pos, color_t color);
bitboard_t position_rook_pieces(position_t* pos, color_t color);
bitboard_t position_bishop_pieces(position_t* pos, color_t color);
bitboard_t position_pinned(position_t* pos);
bitboard_t position_hidden_checkers(position_t* pos);
bitboard_t position_attacks_from(position_t* pos, int square, int piece, bitboard_t occ);
bitboard_t position_attackers_to(position_t* pos, int square);
bitboard_t position_attackers_to_ext(position_t* pos, int square, bitboard_t occ);
bitboard_t position_checkers(position_t* pos);


void position_get_checkinfo(position_t* pos);
int position_move_gives_check(position_t* pos, move_t move);
int position_in_check(position_t* pos);
int position_is_draw(position_t* pos, int checkRepetition, int check3Fold);
int position_pseudo_is_legal(position_t* pos, move_t move);
int position_move_is_pseudo(position_t* pos, move_t move);
int position_move_is_legal(position_t* pos, move_t move);
int position_see_sign(position_t* pos, move_t move);
int position_see(position_t* pos, move_t move);
int position_is_cap_or_prom(position_t* pos, move_t move);
int position_is_cap(position_t* pos, move_t move);
int position_pawns_on7(position_t* pos, color_t stm);

void position_check_vars(position_t* pos);

#endif
