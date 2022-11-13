#include <stdio.h>
#include "move.h"
#include "position.h"
#include "bitboard.h"
#include "stats.h"

enum {
    M_CAPTURE,
    M_QUIET,
    M_QUIET_CHECK,
    M_EVASION,
    M_NON_EVASION,
    M_LEGAL,
};


static int Type = M_NON_EVASION;
static color_t Cur;
static bitboard_t AllPieces;
static bitboard_t Target;
static move_stack_t* Moves;

#define MAKE_STANDARD_MOVES(bb, from)  \
    while (bb) \
    { \
        int to; \
        PLSB(to, bb); \
        (move++)->move = MAKE_MOVE((from), to); \
    }

static move_stack_t* move_generate_castle(position_t* pos, move_stack_t* move)
{
    bitboard_t enemyPieces = pos->color[1 ^ Cur];
    int i1;
    
    ASSERT_IF_FAIL(!position_in_check(pos), "position in check!");
    
    if (Cur == C_WHITE)
    {
        if ((pos->state->castleRights & CASTLE_WHITE_OOO) &&
            (AllPieces & B_Between[A1][E1]) == 0)
        {
            for (i1 = C1; i1 < E1; i1++)
            {
                if (position_attackers_to(pos, i1) & enemyPieces)
                    break;
            }
            if (i1 == E1)
            {
                (move++)->move = MAKE_MOVE_TYPE(E1, C1, M_CASTLE, P_KNIGHT);
            }
        }
        if ((pos->state->castleRights & CASTLE_WHITE_OO) &&
            (AllPieces & B_Between[H1][E1]) == 0)
        {
            for (i1 = G1; i1 > E1; i1--)
            {
                if (position_attackers_to(pos, i1) & enemyPieces)
                    break;
            }
            if (i1 == E1)
            {
                (move++)->move = MAKE_MOVE_TYPE(E1, G1, M_CASTLE, P_KNIGHT);
            }
        }
    }
    else
    {
        if ((pos->state->castleRights & CASTLE_BLACK_OOO) &&
            (AllPieces & B_Between[A8][E8]) == 0)
        {
            for (i1 = C8; i1 < E8; i1++)
            {
                if (position_attackers_to(pos, i1) & enemyPieces)
                    break;
            }
            if (i1 == E8)
            {
                (move++)->move = MAKE_MOVE_TYPE(E8, C8, M_CASTLE, P_KNIGHT);
            }
        }
        if ((pos->state->castleRights & CASTLE_BLACK_OO) &&
            (AllPieces & B_Between[H8][E8]) == 0)
        {
            for (i1 = G8; i1 > E8; i1--)
            {
                if (position_attackers_to(pos, i1) & enemyPieces)
                    break;
            }
            if (i1 == E8)
            {
                (move++)->move = MAKE_MOVE_TYPE(E8, G8, M_CASTLE, P_KNIGHT);
            }
        }
    }
    return move;
}


#if USE_PIECE_LIST

static move_stack_t* move_generate_king(position_t* pos, move_stack_t* move)
{
    int from = pos->piece[Cur][P_KING][0];
    bitboard_t attacks = B_KingAttacks[from] & Target;
    MAKE_STANDARD_MOVES(attacks, from);
    return move;
}

static move_stack_t* move_generate_queen(position_t* pos, move_stack_t* move)
{
    int* pieces = pos->piece[Cur][P_QUEEN];
    if (*pieces != SQ_NONE) do
    {
        bitboard_t attacks =
            (sliding_attack_rook(*pieces, AllPieces) | sliding_attack_bishop(*pieces, AllPieces)) & Target;
        MAKE_STANDARD_MOVES(attacks, (*pieces));
    } while (*++pieces != SQ_NONE);
    return move;
}

static move_stack_t* move_generate_rook(position_t* pos, move_stack_t* move)
{
    int* pieces = pos->piece[Cur][P_ROOK];
    int from;
    for (from = *pieces; from != SQ_NONE; from = *++pieces)
    {
        bitboard_t attacks = sliding_attack_rook(from, AllPieces) & Target;
        MAKE_STANDARD_MOVES(attacks, from);
    }
    return move;
}

static move_stack_t* move_generate_bishop(position_t* pos, move_stack_t* move)
{
    int* pieces = pos->piece[Cur][P_BISHOP];
    int from;
    for (from = *pieces; from != SQ_NONE; from = *++pieces)
    {
        bitboard_t attacks = sliding_attack_bishop(from, AllPieces) & Target;
        MAKE_STANDARD_MOVES(attacks, from);
    }
    return move;
}

#else

static move_stack_t* move_generate_king(position_t* pos, move_stack_t* move)
{
    int from = GLSB(pos->color[Cur] & pos->type[P_KING]);
    bitboard_t attacks = B_KingAttacks[from] & Target;
    MAKE_STANDARD_MOVES(attacks, from);
    return move;
}

static move_stack_t* move_generate_queen(position_t* pos, move_stack_t* move)
{
    bitboard_t pieces = pos->color[Cur] & pos->type[P_QUEEN];
    
    while (pieces)
    {
        int from;
        PLSB(from, pieces);
        bitboard_t attacks =
            (sliding_attack_rook(from, AllPieces) | sliding_attack_bishop(from, AllPieces)) & Target;
        MAKE_STANDARD_MOVES(attacks, from);
    }
    return move;
}

static move_stack_t* move_generate_rook(position_t* pos, move_stack_t* move)
{
    bitboard_t pieces = pos->color[Cur] & pos->type[P_ROOK];
    
    while (pieces)
    {
        int from;
        PLSB(from, pieces);
        bitboard_t attacks = sliding_attack_rook(from, AllPieces) & Target;
        MAKE_STANDARD_MOVES(attacks, from);
    }
    return move;
}

static move_stack_t* move_generate_bishop(position_t* pos, move_stack_t* move)
{
    bitboard_t pieces = pos->color[Cur] & pos->type[P_BISHOP];
    
    while (pieces)
    {
        int from;
        PLSB(from, pieces);
        bitboard_t attacks = sliding_attack_bishop(from, AllPieces) & Target;
        MAKE_STANDARD_MOVES(attacks, from);
    }
    return move;
}
#endif

static move_stack_t* move_generate_knight(position_t* pos, move_stack_t* move)
{
    bitboard_t pieces = pos->color[Cur] & pos->type[P_KNIGHT];
    
    while (pieces)
    {
        int from;
        PLSB(from, pieces);
        bitboard_t attacks = B_KnightAttacks[from] & Target;
        MAKE_STANDARD_MOVES(attacks, from);
    }
    return move;
}

#define MAKE_PAWN_MOVES(bb, dir)  \
    while (bb) \
    { \
        int to; \
        PLSB(to, bb); \
        (move++)->move = MAKE_MOVE(to - (dir), to); \
    }
    
#define MAKE_EN_PASSANT(bb, dir)  \
    while (bb) \
    { \
        int to; \
        PLSB(to, bb); \
        (move++)->move = MAKE_MOVE_TYPE(to - (dir), to, M_ENPASSANT, P_KNIGHT); \
    }
    
#define MAKE_PROMOTION(bb, dir) \
    while (bb) \
    { \
        int to; \
        PLSB(to, bb); \
        int from = to - (dir); \
        (move++)->move = MAKE_MOVE_TYPE(from, to, M_PROMOTION, P_QUEEN); \
        (move++)->move = MAKE_MOVE_TYPE(from, to, M_PROMOTION, P_ROOK); \
        (move++)->move = MAKE_MOVE_TYPE(from, to, M_PROMOTION, P_BISHOP); \
        (move++)->move = MAKE_MOVE_TYPE(from, to, M_PROMOTION, P_KNIGHT); \
    }

//            if (Type == M_CAPTURE) break;

static move_stack_t* move_generate_pawn(position_t* board, move_stack_t* move)
{
    color_t opp = Cur ^ 1;
    bitboard_t pawns = board->type[P_PAWN] & board->color[Cur];
    bitboard_t rank7 = (Cur == C_WHITE ? B_Rank7 : B_Rank2);
    bitboard_t rank3 = (Cur == C_WHITE ? B_Rank3 : B_Rank6);
    bitboard_t fileA = (Cur == C_WHITE ? B_FileA : B_FileH);
    bitboard_t fileH = (Cur == C_WHITE ? B_FileH : B_FileA);
    bitboard_t pawnsOn7 = (pawns & rank7);
    bitboard_t pawnsNotOn7 = (pawns & ~rank7);
    bitboard_t emptySquares = ~(board->color[Cur] | board->color[opp]);
    bitboard_t enemyTarget = board->color[opp] & Target;  // enemies within target
    bitboard_t emptyTarget = emptySquares & Target;       // empty squares within target
    int up = (Cur == C_WHITE ? DELTA_N : DELTA_S);
    int left = (Cur == C_WHITE ? DELTA_NW : DELTA_SE);
    int right = (Cur == C_WHITE ? DELTA_NE : DELTA_SW);
    
    // single and double pawn pushes, no promotions
    if (pawnsNotOn7 && emptyTarget)
    {
        bitboard_t b1 = (Cur == C_WHITE ? pawnsNotOn7 << 8 : pawnsNotOn7 >> 8) & emptySquares;
        bitboard_t b2 = (Cur == C_WHITE ? (b1 & rank3) << 8 : (b1 & rank3) >> 8) & emptyTarget;
        b1 &= Target;

        MAKE_PAWN_MOVES(b2, up + up);
        MAKE_PAWN_MOVES(b1, up);
    }
    
    // promotions
    if (pawnsOn7)
    {
        if (enemyTarget)
        {
            bitboard_t bl =
                (Cur == C_WHITE ? (pawnsOn7 & ~fileA) << 7 : (pawnsOn7 & ~fileA) >> 7) & enemyTarget;
            bitboard_t br =
                (Cur == C_WHITE ? (pawnsOn7 & ~fileH) << 9 : (pawnsOn7 & ~fileH) >> 9) & enemyTarget;
            MAKE_PROMOTION(bl, left);
            MAKE_PROMOTION(br, right);
        }
        if (emptyTarget)
        {
            bitboard_t bu =
                (Cur == C_WHITE ? pawnsOn7 << 8 : pawnsOn7 >> 8) & emptyTarget;
            MAKE_PROMOTION(bu, up);
        }
    }
    
    // captures
    if (pawnsNotOn7 && enemyTarget)
    {
        // standard captures
        bitboard_t b1 =
            (Cur == C_WHITE ? (pawnsNotOn7 & ~fileA) << 7 : (pawnsNotOn7 & ~fileA) >> 7);
        bitboard_t b2 =
            (Cur == C_WHITE ? (pawnsNotOn7 & ~fileH) << 9 : (pawnsNotOn7 & ~fileH) >> 9);
        bitboard_t bl = b1 & enemyTarget;
        bitboard_t br = b2 & enemyTarget;
        
        MAKE_PAWN_MOVES(bl, left);
        MAKE_PAWN_MOVES(br, right);

        // en-passant captures
        if (board->state->enPassant != SQ_NONE)
        {
            bitboard_t pp = BB(board->state->enPassant - up);
            if (pp & enemyTarget)
            {
                bitboard_t ep = BB(board->state->enPassant);
                bl = b1 & ep;
                br = b2 & ep;
                MAKE_EN_PASSANT(bl, left);
                MAKE_EN_PASSANT(br, right);
            }
        }
    }

    return move;
}

static move_stack_t* move_generate_pawn_quiet(position_t* board, move_stack_t* move)
{
    bitboard_t pawns = board->type[P_PAWN] & board->color[Cur];
    bitboard_t rank7 = (Cur == C_WHITE ? B_Rank7 : B_Rank2);
    bitboard_t pawnsNotOn7 = (pawns & ~rank7);
    bitboard_t emptySquares = ~AllPieces;
    int up = (Cur == C_WHITE ? DELTA_N : DELTA_S);
    
    // single and double pawn pushes, no promotions
    if (pawnsNotOn7)
    {
        bitboard_t b1 = (Cur == C_WHITE ? pawnsNotOn7 << 8 : pawnsNotOn7 >> 8) & emptySquares;
        bitboard_t b2 = (Cur == C_WHITE ? (b1 & B_Rank3) << 8 : (b1 & B_Rank6) >> 8) & emptySquares;

        MAKE_PAWN_MOVES(b2, up + up);
        MAKE_PAWN_MOVES(b1, up);
    }
    
    return move;
}

static move_stack_t* move_generate_pawn_capture(position_t* board, move_stack_t* move)
{
    color_t opp = Cur ^ 1;
    bitboard_t pawns = board->type[P_PAWN] & board->color[Cur];
    bitboard_t rank7 = (Cur == C_WHITE ? B_Rank7 : B_Rank2);
    bitboard_t pawnsOn7 = (pawns & rank7);
    bitboard_t pawnsNotOn7 = (pawns & ~rank7);
    bitboard_t emptySquares = ~AllPieces;
    bitboard_t enemies = board->color[opp];
    int up = (Cur == C_WHITE ? DELTA_N : DELTA_S);
    int left = (Cur == C_WHITE ? DELTA_NW : DELTA_SE);
    int right = (Cur == C_WHITE ? DELTA_NE : DELTA_SW);
    
    // promotions
    if (pawnsOn7)
    {
        bitboard_t bl =
            (Cur == C_WHITE ? (pawnsOn7 & B_NotFileA) << 7 : (pawnsOn7 & B_NotFileH) >> 7) & enemies;
        bitboard_t br =
            (Cur == C_WHITE ? (pawnsOn7 & B_NotFileH) << 9 : (pawnsOn7 & B_NotFileA) >> 9) & enemies;
        MAKE_PROMOTION(bl, left);
        MAKE_PROMOTION(br, right);

        bitboard_t bu =
            (Cur == C_WHITE ? pawnsOn7 << 8 : pawnsOn7 >> 8) & emptySquares;
        MAKE_PROMOTION(bu, up);
    }
    
    // captures
    if (pawnsNotOn7)
    {
        // standard captures
        bitboard_t b1 =
            (Cur == C_WHITE ? (pawnsNotOn7 & B_NotFileA) << 7 : (pawnsNotOn7 & B_NotFileH) >> 7);
        bitboard_t b2 =
            (Cur == C_WHITE ? (pawnsNotOn7 & B_NotFileH) << 9 : (pawnsNotOn7 & B_NotFileA) >> 9);
        bitboard_t bl = b1 & enemies;
        bitboard_t br = b2 & enemies;
        
        MAKE_PAWN_MOVES(bl, left);
        MAKE_PAWN_MOVES(br, right);

        // en-passant captures
        if (board->state->enPassant != SQ_NONE)
        {
            bitboard_t pp = BB(board->state->enPassant - up);
            if (pp & enemies)
            {
                bitboard_t ep = BB(board->state->enPassant);
                bl = b1 & ep;
                br = b2 & ep;
                MAKE_EN_PASSANT(bl, left);
                MAKE_EN_PASSANT(br, right);
            }
        }
    }

    return move;
}

move_stack_t* move_generate_quiet(position_t* pos, move_stack_t* move)
{
    Moves = move;
    Type = M_QUIET;
    Cur = pos->to_move;
    AllPieces = pos->color[C_WHITE] | pos->color[C_BLACK];
    Target = ~AllPieces;

    move = move_generate_pawn_quiet(pos, move);
    move = move_generate_knight(pos, move);
    move = move_generate_queen(pos, move);
    move = move_generate_rook(pos, move);
    move = move_generate_bishop(pos, move);
    move = move_generate_king(pos, move);
    move = move_generate_castle(pos, move);

    stats_inc(ST_MOVE_GEN, move - Moves);
    return move;
}

move_stack_t* move_generate_capture(position_t* pos, move_stack_t* move)
{
    Moves = move;
    Type = M_CAPTURE;
    Cur = pos->to_move;
    AllPieces = pos->color[C_WHITE] | pos->color[C_BLACK];
    Target = pos->color[1 ^ Cur];

    move = move_generate_pawn_capture(pos, move);
    move = move_generate_knight(pos, move);
    move = move_generate_queen(pos, move);
    move = move_generate_rook(pos, move);
    move = move_generate_bishop(pos, move);
    move = move_generate_king(pos, move);

    stats_inc(ST_MOVE_GEN, move - Moves);
    return move;
}

move_stack_t* move_generate_non_evasion(position_t* pos, move_stack_t* move)
{
    Moves = move;
    Type = M_NON_EVASION;
    Cur = pos->to_move;
    AllPieces = pos->color[C_WHITE] | pos->color[C_BLACK];
    Target = ~pos->color[Cur];

    move = move_generate_pawn_capture(pos, move);
    move = move_generate_pawn_quiet(pos, move);
//    move = move_generate_pawn(pos, move);
    move = move_generate_knight(pos, move);
    move = move_generate_bishop(pos, move);
    move = move_generate_rook(pos, move);
    move = move_generate_queen(pos, move);
    move = move_generate_king(pos, move);
    move = move_generate_castle(pos, move);

    stats_inc(ST_MOVE_GEN, move - Moves);
    return move;
}

move_stack_t* move_generate_evasion(position_t* pos, move_stack_t* move)
{
    bitboard_t checkers = pos->state->checkers;
    bitboard_t b = checkers;
    bitboard_t sliderAttacks = 0;
    int kingSquare = position_king_square(pos, pos->to_move);
    unsigned cnt = 0;
    int square = 0;
    
    ASSERT_IF_FAIL(checkers, "checkers expected");

    Moves = move;
    Type = M_EVASION;
    Cur = pos->to_move;
    AllPieces = pos->color[Cur] | pos->color[1 ^ Cur];
    
    while (b)
    {
        PLSB(square, b);
        int piece = pos->board[square];
        
        ASSERT_IF_FAIL(pos->color[1 ^ pos->to_move] & BB(square), "wrong checker color");
        ASSERT_IF_FAIL(piece != P_NONE, "no piece on checker square");
        
        cnt++;

        if (piece == P_BISHOP)
        {
            sliderAttacks |= B_BishopAttacks[square];
        }
        else if (piece == P_ROOK)
        {
            sliderAttacks |= B_RookAttacks[square];
        }
        else if (piece == P_QUEEN)
        {
            if (B_Between[kingSquare][square] || !(B_BishopAttacks[square] & BB(kingSquare)))
            {
                sliderAttacks |= B_QueenAttacks[square];
            }
            else
            {
                sliderAttacks |= B_BishopAttacks[square] | sliding_attack_rook(square, AllPieces);
            }
        }
    }

    b = B_KingAttacks[kingSquare] & ~pos->color[Cur] & ~sliderAttacks;
    MAKE_STANDARD_MOVES(b, kingSquare);

    if (cnt <= 1)
    {
        // single check, block or capture the checker is another option
        Target = B_Between[kingSquare][square] | checkers;

        move = move_generate_pawn(pos, move);
        move = move_generate_knight(pos, move);
        move = move_generate_bishop(pos, move);
        move = move_generate_rook(pos, move);
        move = move_generate_queen(pos, move);
    }

    stats_inc(ST_MOVE_GEN, move - Moves);
    return move;
}

/*
move_stack_t* move_generate_quiet_check(position_t* pos, move_stack_t* move)
{
    bitboard_t hCheckers;
    
    Moves = move;
    Type = M_QUIET_CHECK;
    Cur = pos->to_move;
    AllPieces = pos->color[Cur] | pos->color[1 ^ Cur];

    position_get_checkinfo(pos);
    hCheckers = pos->state->hCheckers;
    
    while (hCheckers)
    {
        int from;
        int piece;
        bitboard_t b;
                
        PLSB(from, hCheckers);
        piece = pos->board[from];

        if (piece == P_PAWN)
            continue;

        b = position_attacks_from(pos, from, piece, AllPieces) & ~AllPieces;

        if (piece == P_KING)
            b &= ~B_QueenAttacks[position_king_square(pos, Cur)];

        MAKE_STANDARD_MOVES(b, from);
    }

    Target = ~AllPieces;

    move = move_generate_pawn_capture(pos, move);
    move = move_generate_pawn_quiet(pos, move);
    move = move_generate_knight(pos, move);
    move = move_generate_bishop(pos, move);
    move = move_generate_rook(pos, move);
    move = move_generate_queen(pos, move);
    move = move_generate_castle(pos, move);

    stats_inc(ST_MOVE_GEN, move - Moves);
    return move;
}
*/

move_stack_t* move_generate_legal(position_t* pos, move_stack_t* move)
{
    move_stack_t* result;
    int kingSquare = position_king_square(pos, pos->to_move);
    
    if (position_in_check(pos))
        result = move_generate_evasion(pos, move);
    else
        result = move_generate_non_evasion(pos, move);
    
    position_get_checkinfo(pos);
    while (move != result)
    {
        if (pos->state->pinned == 0 &&
            MOVE_FROM(move->move) != kingSquare &&
            MOVE_TYPE(move->move) != M_ENPASSANT)
            move++;
        else if (position_pseudo_is_legal(pos, move->move))
            move++;
        else
            move->move = (--result)->move;
    }

    return result;
}

