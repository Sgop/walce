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
        if (pos->pCount[C_WHITE][P_BISHOP] == 2)
            res += PieceValue[P_PAWN][PHASE_MG] / 2;
        if (pos->pCount[C_BLACK][P_BISHOP] == 2)
            res -= PieceValue[P_PAWN][PHASE_MG] / 2;
#endif
    }
    return res;
}

static int eval_mobility(position_t* pos)
{
    int res = 0;
#if USE_PIECE_LIST
    const int mBonus[C_NUM][PHASE_NUM][P_NUM] = {
        { {0, 0, +2, +5, +3, +5, 0}, {0, 0, +2, +3, +4, +5, 0}, },
        { {0, 0,- 2, -5, -3, -5, 0}, {0, 0, -2, -3, -4, -5, 0}, },
    };
    bitboard_t occ = pos->color[C_WHITE] | pos->color[C_BLACK];
    color_t color;
        
    for (color = C_WHITE; color <= C_BLACK; color++)
    {
        bitboard_t target = ~pos->color[color];
        int* pieces;
        int temp, square;

        pieces = pos->piece[color][P_KNIGHT];
        for (temp = 0, square = *pieces; *pieces != SQ_NONE; pieces++)
            temp += popcount(B_KnightAttacks[square] & target);
        res += temp * mBonus[color][Phase][P_KNIGHT];
    
        pieces = pos->piece[color][P_BISHOP];
        for (temp = 0, square = *pieces; *pieces != SQ_NONE; pieces++)
            temp += popcount(sliding_attack_bishop(square, occ) & target);
        res += temp * mBonus[color][Phase][P_BISHOP];

        pieces = pos->piece[color][P_ROOK];
        for (temp = 0, square = *pieces; *pieces != SQ_NONE; pieces++)
            temp += popcount(sliding_attack_rook(square, occ) & target);
        res += temp * mBonus[color][Phase][P_ROOK];

        pieces = pos->piece[color][P_QUEEN];
        for (temp = 0, square = *pieces; *pieces != SQ_NONE; pieces++)
            temp += popcount((sliding_attack_bishop(square, occ) | sliding_attack_bishop(square, occ)) & target);
        res += temp * mBonus[color][Phase][P_QUEEN];
    }
#endif

    return res;
}

static int eval_pawn(position_t* pos)
{
    int res = 0;
#if USE_PIECE_LIST
    bitboard_t wPawns = pos->type[P_PAWN] & pos->color[C_WHITE];
    bitboard_t bPawns = pos->type[P_PAWN] & pos->color[C_BLACK];
    bitboard_t allPawns = pos->type[P_PAWN];
    bitboard_t b;
    int square;
    int* pieces;
    
    pieces = pos->piece[C_WHITE][P_PAWN];
    while ((square = *pieces++) != SQ_NONE)
    {
        int r = RANK_OF(square);
        int f = FILE_OF(square);
        int cPawn = (wPawns & B_AdjacentFile[f] & (B_Rank[r] | B_Rank[r-1])) != 0;
        int iPawn = (wPawns & B_AdjacentFile[f]) == 0;
        int dPawn = (wPawns & B_Forward[C_WHITE][square]) != 0;
        int oPawn = (bPawns & B_Forward[C_WHITE][square]) != 0;
        int pPawn = (bPawns & B_PassedPawnMask[C_WHITE][square]) == 0;
        int bPawn = 0;
        int candidate = 0;
        
        if (!pPawn && !iPawn && !cPawn &&
            !(wPawns & B_AttackSpawnMask[C_BLACK][square]) &&
            !(bPawns & B_WPawnAttacks[square]))
        {
            b = B_WPawnAttacks[square];
            while (!(b & allPawns))
                b <<= 8;
            bPawn = ((b | (b << 8)) & bPawns) != 0;
        }

        candidate =
            !oPawn && !pPawn && !bPawn && !iPawn &&
            (b = B_AttackSpawnMask[C_BLACK][square + 8] & wPawns) != 0 &&
            popcount_max15(b) >= popcount_max15(B_AttackSpawnMask[C_WHITE][square] & bPawns);

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

    pieces = pos->piece[C_BLACK][P_PAWN];
    while ((square = *pieces++) != SQ_NONE)
    {
        int r = RANK_OF(square);
        int f = FILE_OF(square);
        int cPawn = (bPawns & B_AdjacentFile[f] & (B_Rank[r] | B_Rank[r+1])) != 0;
        int iPawn = (bPawns & B_AdjacentFile[f]) == 0;
        int dPawn = (bPawns & B_Forward[C_BLACK][square]) != 0;
        int oPawn = (wPawns & B_Forward[C_BLACK][square]) != 0;
        int pPawn = (wPawns & B_PassedPawnMask[C_BLACK][square]) == 0;
        int bPawn = 0;
        int candidate = 0;
        
        if (!pPawn && !iPawn && !cPawn &&
            !(bPawns & B_AttackSpawnMask[C_WHITE][square]) &&
            !(wPawns & B_BPawnAttacks[square]))
        {
            b = B_WPawnAttacks[square];
            while (!(b & allPawns))
                b >>= 8;
            bPawn = ((b | (b >> 8)) & wPawns) != 0;
        }

        candidate =
            !oPawn && !pPawn && !bPawn && !iPawn &&
            (b = B_AttackSpawnMask[C_WHITE][square + 8] & bPawns) != 0 &&
            popcount_max15(b) >= popcount_max15(B_AttackSpawnMask[C_BLACK][square] & wPawns);

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

    if (pos->state->npMaterial[C_WHITE] < PieceValue[P_NONE][PHASE_MG] &&
        pos->state->npMaterial[C_BLACK] < PieceValue[P_NONE][PHASE_MG])
        Phase = PHASE_EG;
    else
        Phase = PHASE_MG;

    res += eval_material(pos);
    res += eval_mobility(pos);
    res += eval_pawn(pos);
    
    if (pos->to_move == C_WHITE)
        return res;
    else
        return -res;
}


