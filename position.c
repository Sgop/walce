#include "position.h"
#include "stats.h"
#include "log.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "squaretable.h"
#include "move.h"
#include "prnd.h"

const char P_CHAR[P_NUM] = {' ', 'p', 'n', 'b', 'r', 'q', 'k'};
const int PieceValue[P_NUM][PHASE_NUM] = {
    {3500, 3500},
    {198,  258 },
    {817,  846 },
    {836,  857 },
    {1270, 1278},
    {2521, 2558},
};
const int PieceValue2[P_NUM][PHASE_NUM] = {
    {1200, 1200},
    {100,  100 },
    {250,  250 },
    {300,  300 },
    {550,  550 },
    {1000, 1000},
};
int SquareValue[P_NUM][C_NUM][SQ_NUM];

int CastleMaskFrom[SQ_NUM];
int CastleMaskTo[SQ_NUM];

pkey_t Zob_psq[C_NUM][P_NUM][SQ_NUM];
pkey_t Zob_enpassant[FileNum];
pkey_t Zob_castle[CASTLE_ALL+1];
pkey_t Zob_side;
//pkey_t Zob_exclusion;

static pkey_t position_compute_key(position_t* pos);
static int position_compute_score(position_t* pos);


void positions_init()
{
    int piece;
    color_t color;
    int square;
    int file;
    int cr;
    
    prnd_init();
    
    for (color = C_WHITE; color <= C_BLACK; color++)
        for (piece = P_PAWN; piece <= P_KING; piece++)
            for (square = A1; square <= H8; square++)
                Zob_psq[color][piece][square] = prand64();
    for (file = FileA; file <= FileH; file++)
        Zob_enpassant[file] = prand64();

    for (cr = CASTLE_NONE; cr <= CASTLE_ALL; cr++)
    {
        bitboard_t b = cr;
        while (b)
        {
            int index;
            PLSB(index, b);
            pkey_t k = Zob_castle[1 << index];
            Zob_castle[cr] ^= k ? k : prand64();
        }
    }

    Zob_side = prand64();
    //Zob_exclusion = prand64();

    for (piece = P_PAWN; piece <= P_KING; piece++)
    {
        int baseScore = MAKE_SCORE(PieceValue[piece][PHASE_MG], PieceValue[piece][PHASE_EG]);
        int square;

        for (square = A1; square <= H8; square++)
        {
            int addScore = MAKE_SCORE(PST[piece][PHASE_MG][square], PST[piece][PHASE_EG][square]);
            //addScore = 0;
            SquareValue[piece][C_WHITE][square] = (baseScore + addScore);
            SquareValue[piece][C_BLACK][63-square] = -(baseScore + addScore);
        }
    }
    
    memset(CastleMaskFrom, 0, SQ_NUM * sizeof(*CastleMaskFrom));
    memset(CastleMaskTo, 0, SQ_NUM * sizeof(*CastleMaskTo));
    CastleMaskFrom[E1] = CASTLE_WHITE_OO | CASTLE_WHITE_OOO;
    CastleMaskFrom[E8] = CASTLE_BLACK_OO | CASTLE_BLACK_OOO;
    CastleMaskFrom[A1] = CASTLE_WHITE_OOO;
    CastleMaskTo  [A1] = CASTLE_WHITE_OOO;
    CastleMaskFrom[A8] = CASTLE_BLACK_OOO;
    CastleMaskTo  [A8] = CASTLE_BLACK_OOO;
    CastleMaskFrom[H1] = CASTLE_WHITE_OO;
    CastleMaskTo  [H1] = CASTLE_WHITE_OO;
    CastleMaskFrom[H8] = CASTLE_BLACK_OO;
    CastleMaskTo  [H8] = CASTLE_BLACK_OO;
}

char* move_format(move_t move)
{
    static char result[100];
    int from = MOVE_FROM(move);
    int to = MOVE_TO(move);
    
    if (MOVE_TYPE(move) == M_PROMOTION)
        sprintf(result, "%c%c%c%c%c",
                'a'+  (from % 8), '1' + (from / 8),
                'a'+  (to % 8), '1' + (to / 8),
                P_CHAR[MOVE_PROM(move)]);
    else
        sprintf(result, "%c%c%c%c",
               'a'+  (from % 8), '1' + (from / 8),
            'a'+  (to % 8), '1' + (to / 8));
    return result;
}

position_t* position_new(void)
{
    position_t* pos = malloc(sizeof(position_t));
    position_reset(pos);
    return pos;
}

static void position_clear(position_t* pos)
{
    memset(pos, 0, sizeof(*pos));
    pos->to_move = C_NONE;
    pos->state = pos->states;
    pos->state->enPassant = SQ_NONE;

#if USE_PIECE_LIST
    {
        int i1, i2, i3;

        for (i1 = 0; i1 < C_NUM; i1++)
            for (i2 = 0; i2 < P_NUM; i2++)
                for (i3 = 0; i3 < 16; i3++)
                    pos->piece[i1][i2][i3] = SQ_NONE;
	}
#endif
}

#if USE_PIECE_LIST
#  define UPDATE_PLIST(col, type, i, to) \
    pos->piece[col][type][pos->index[to] = i] = to;
#else
#  define UPDATE_PLIST(Cur, type, i, to)
#endif

static void position_add_piece(position_t* pos, int square, int piece, color_t color)
{
    bitboard_t bs = BB(square);
    
    pos->color[color] |= bs;
    pos->type[piece] |= bs;
    pos->board[square] = piece;
    UPDATE_PLIST(color, piece, pos->pCount[color][piece]++, square);
    pos->state->score += SquareValue[piece][color][square];
    if (piece != P_PAWN)
        pos->state->npMaterial[color] += PieceValue[piece][PHASE_MG];
    pos->state->key ^= Zob_psq[color][piece][square];
}

void position_reset(position_t* pos)
{
    position_set(pos, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void position_destroy(position_t* pos)
{
    free(pos);
}

void position_print(position_t* pos, color_t color)
{
    int rank, file;
    FILE* fd = log_get_fd();
    
    if (!fd)
        return;
        
    if (color == C_WHITE)
    {
        fprintf(fd, "\033[40m                                         \033[0m\n");
        for (rank = Rank8; rank >= Rank1; rank--)
        {
            fprintf(fd, "\033[40m   ");
            fprintf(fd, "\033[47m +---+---+---+---+---+---+---+---+ ");
            fprintf(fd, "\033[40m   \033[0m\n");
            fprintf(fd, "\033[1;37;40m %d ", rank+1);
            fprintf(fd, "\033[47m");
            for (file = FileA; file <= FileH; file++)
            {
                int square = SQ(file, rank);
                bitboard_t mask = BB(square);
                fprintf(fd, "\033[1;30m |");
                if (pos->color[C_BLACK] & mask)
                    fprintf(fd, "\033[1;30m");
                else
                    fprintf(fd, "\033[1;31m");
                    
                if (pos->type[P_PAWN] & mask)
                    fprintf(fd, " P");
                else if (pos->type[P_ROOK] & mask)
                    fprintf(fd, " R");
                else if (pos->type[P_KNIGHT] & mask)
                    fprintf(fd, " N");
                else if (pos->type[P_BISHOP] & mask)
                    fprintf(fd, " B");
                else if (pos->type[P_QUEEN] & mask)
                    fprintf(fd, " Q");
                else if (pos->type[P_KING] & mask)
                    fprintf(fd, " K");
                else
                    fprintf(fd, "  ");
            }
            fprintf(fd, "\033[1;30m | ");
            fprintf(fd, "\033[40m   \033[0m\n");
        }
        fprintf(fd, "\033[40m   ");
        fprintf(fd, "\033[47m +---+---+---+---+---+---+---+---+ ");
        fprintf(fd, "\033[40m   \033[0m\n");
        fprintf(fd, "\033[1;37;40m      a   b   c   d   e   f   g   h      \033[0m\n");
    }
    else if (color == C_BLACK)
    {
        fprintf(fd, "\033[40m                                         \033[0m\n");
        for (rank = Rank1; rank <= Rank8; rank++)
        {
            fprintf(fd, "\033[40m   ");
            fprintf(fd, "\033[47m +---+---+---+---+---+---+---+---+ ");
            fprintf(fd, "\033[40m   \033[0m\n");
            fprintf(fd, "\033[1;37;40m %d ", rank+1);
            fprintf(fd, "\033[47m");
            for (file = FileH; file >= FileA; file--)
            {
                int square = SQ(file, rank);
                bitboard_t mask = BB(square);
                fprintf(fd, "\033[1;30m |");
                if (pos->color[C_BLACK] & mask)
                    fprintf(fd, "\033[1;30m");
                else
                    fprintf(fd, "\033[1;31m");
                    
                if (pos->type[P_PAWN] & mask)
                    fprintf(fd, " P");
                else if (pos->type[P_ROOK] & mask)
                    fprintf(fd, " R");
                else if (pos->type[P_KNIGHT] & mask)
                    fprintf(fd, " N");
                else if (pos->type[P_BISHOP] & mask)
                    fprintf(fd, " B");
                else if (pos->type[P_QUEEN] & mask)
                    fprintf(fd, " Q");
                else if (pos->type[P_KING] & mask)
                    fprintf(fd, " K");
                else
                    fprintf(fd, "  ");
            }
            fprintf(fd, "\033[1;30m | ");
            fprintf(fd, "\033[40m   \033[0m\n");
        }
        fprintf(fd, "\033[40m   ");
        fprintf(fd, "\033[47m +---+---+---+---+---+---+---+---+ ");
        fprintf(fd, "\033[40m   \033[0m\n");
        fprintf(fd, "\033[1;37;40m      h   g   f   e   d   c   b   a      \033[0m\n");
    }
    else
    {
        fprintf(fd, "error\n");
    }
}

static void printFenError(const char* fen, int index, const char* error)
{
    log_line("Invalid FEN string at position %d", index+1);
    log_line("%s", fen);
    log_line("%*c", index+1, '^');
    log_line("%*c%s", index, ' ', error);
    log_line("");
}

int position_set(position_t* position, const char* fen)
{
    const char* pos = fen;
    int rank = 7;
    int file = 0;
    int last_was_empty = 0;
    int dash = 0;
    
    position_clear(position);
    
    // piece placement
    while (pos && *pos)
    {
        if ('1' <= *pos && *pos <= '8')
        {
            int empty = *pos - '0';
            if (empty + file > 8)
            {
                printFenError(fen, pos-fen, "Invalid number of empty squares");
                return 0;
            }
            else if (last_was_empty)
            {
                printFenError(fen, pos-fen, "Empty squares must not follow empty squares");
                return 0;
            }
            last_was_empty = 1;
            file += empty;
        }
        else if (*pos == '/')
        {
            if (file != 8)
            {
                printFenError(fen, pos-fen, "Unexpected start of new rank");
                return 0;
            }
            else if (rank == 0)
            {
                printFenError(fen, pos-fen, "No more rank expected");
                return 0;
            }
            file = 0;
            rank--;
            last_was_empty = 0;
        }
        else if (*pos == ' ')
        {
            pos++;
            break;
        }
        else if (file < 8)
        {
            if (*pos == 'p')
                position_add_piece(position, SQ(file, rank), P_PAWN, C_BLACK);
            else if (*pos == 'P')
                position_add_piece(position, SQ(file, rank), P_PAWN, C_WHITE);
            else if (*pos == 'r')
                position_add_piece(position, SQ(file, rank), P_ROOK, C_BLACK);
            else if (*pos == 'R')
                position_add_piece(position, SQ(file, rank), P_ROOK, C_WHITE);
            else if (*pos == 'n')
                position_add_piece(position, SQ(file, rank), P_KNIGHT, C_BLACK);
            else if (*pos == 'N')
                position_add_piece(position, SQ(file, rank), P_KNIGHT, C_WHITE);
            else if (*pos == 'b')
                position_add_piece(position, SQ(file, rank), P_BISHOP, C_BLACK);
            else if (*pos == 'B')
                position_add_piece(position, SQ(file, rank), P_BISHOP, C_WHITE);
            else if (*pos == 'q')
                position_add_piece(position, SQ(file, rank), P_QUEEN, C_BLACK);
            else if (*pos == 'Q')
                position_add_piece(position, SQ(file, rank), P_QUEEN, C_WHITE);
            else if (*pos == 'k')
                position_add_piece(position, SQ(file, rank), P_KING, C_BLACK);
            else if (*pos == 'K')
                position_add_piece(position, SQ(file, rank), P_KING, C_WHITE);
            else
            {
                printFenError(fen, pos-fen, "Invalid character");
                return 0;
            }
            file++;
            last_was_empty = 0;
        }
        else
        {
            printFenError(fen, pos-fen, "Invalid character at end of file");
            return 0;
        }
        pos++;
    }
    if (file != 8 || rank != 0)
    {
        printFenError(fen, pos-fen, "Unexpected end of piece placement");
        return 0;
    }

    // side to move
    while (pos && *pos)
    {
        if (*pos == ' ')
        {
            pos++;
            break;
        }
        else if (position->to_move == C_NONE)
        {
            if (*pos == 'w')
                position->to_move = C_WHITE;
            else if (*pos == 'b')
            {
                position->to_move = C_BLACK;
                position->state->key ^= Zob_side;
            }
            else
            {
                printFenError(fen, pos-fen, "Invalid side to move");
                return 0;
            }
        }
        else
        {
            printFenError(fen, pos-fen, "Unexpected character for side to move");
            return 0;
        }
        pos++;
    }
    if (position->to_move == C_NONE)
    {
        printFenError(fen, pos-fen, "No side to move");
        return 0;
    }
        
    // castle rights
    dash = 0;
    while (pos && *pos)
    {
        if (*pos == ' ')
        {
            pos++;
            break;
        }
        else if (dash)
        {
            printFenError(fen, pos-fen, "Unexpected character for castle rights");
            return 0;
        }
        else if (*pos == '-')
        {
            dash = 1;
        }
        else if (*pos == 'K')
        {
            position->state->castleRights |= CASTLE_WHITE_OO;
        }
        else if (*pos == 'Q')
        {
            position->state->castleRights |= CASTLE_WHITE_OOO;
        }
        else if (*pos == 'k')
        {
            position->state->castleRights |= CASTLE_BLACK_OO;
        }
        else if (*pos == 'q')
        {
            position->state->castleRights |= CASTLE_BLACK_OOO;
        }
        else
        {
            printFenError(fen, pos-fen, "Unexpected character for castle rights");
            return 0;
        }
        pos++;
    }
    if (!dash && position->state->castleRights == CASTLE_NONE)
    {
        printFenError(fen, pos-fen, "Invalid castle rights");
        return 0;
    }
    position->state->key ^= Zob_castle[position->state->castleRights];
    
    // en-passant
    dash = 0;
    while (pos && *pos)
    {
        if (*pos == ' ')
        {
            pos++;
            break;
        }
        else if (dash)
        {
            printFenError(fen, pos-fen, "Unexpected character for en-passant");
            return 0;
        }
        else if (*pos == '-')
        {
            dash = 1;
        }
        else if ('a' <= *pos && *pos <= 'h' && (pos[1] == '3' || pos[1] == '6'))
        {
            int file = pos[0] - 'a';
            int rank = pos[1] - '1';
            position->state->enPassant = SQ(file, rank);
            position->state->key = Zob_enpassant[file];
            pos++;
        }
        else
        {
            printFenError(fen, pos-fen, "Unexpected square for en-passant");
            return 0;
        }
        pos++;
    }
    if (!dash && position->state->enPassant == SQ_NONE)
    {
        printFenError(fen, pos-fen, "Invalid en-passant");
        return 0;
    }

    // half move clock
    while (pos && *pos)
    {
        if (*pos == ' ')
        {
            pos++;
            break;
        }
        else if ('0' <= *pos && *pos <= '9')
        {
            int num = pos[0] - '0';
            position->state->rule50 = position->state->rule50 * 10 + num;
        }
        else
        {
            printFenError(fen, pos-fen, "Invalid character for halfmove clock");
            return 0;
        }
        pos++;
    }
    
    // full move counter
    while (pos && *pos)
    {
        if ('1' <= *pos && *pos <= '9')
        {
            int num = pos[0] - '0';
            position->state->plyNum = position->state->plyNum * 10 + num;
        }
        else if (position->state->plyNum == 0)
        {
            printFenError(fen, pos-fen, "Invalid character for fullmove counter");
            return 0;
        }
        else if (*pos == '0')
        {
            position->state->plyNum *= 10;
        }
        else
        {
            printFenError(fen, pos-fen, "Unexpected character for fullmove counter");
            return 0;
        }
        pos++;
    }
    if (position->state->plyNum)
    {
        position->state->plyNum *= 2;
        if (position->to_move == C_WHITE)
            position->state->plyNum -= 2;
        else 
            position->state->plyNum -= 1;
    }
    
    position->state->checkers = position_checkers(position);
    //position->state->pinned = position_pinned(position);
    //position_get_checkinfo(position);
    
    return 1;
}

char* position_get(position_t* pos)
{
    static char result[1024];
    char* p = result;
    int rank, file;
    
    // piece placement
    for (rank = 7; rank >= 0; rank--)
    {
        int empty = 0;
        for (file = 0; file < 8; file++)
        {
            int square = SQ(file, rank);
            if (pos->board[square] == P_NONE)
            {
                empty++;
            }
            else
            {
                if (empty > 0)
                {
                    *p++ = '0' + empty;
                    empty = 0;
                }
                if (pos->color[C_WHITE] & BB(square))
                {
                    if (pos->board[square] == P_PAWN)
                        *p++ = 'P';
                    if (pos->board[square] == P_ROOK)
                        *p++ = 'R';
                    if (pos->board[square] == P_KNIGHT)
                        *p++ = 'N';
                    if (pos->board[square] == P_BISHOP)
                        *p++ = 'B';
                    if (pos->board[square] == P_QUEEN)
                        *p++ = 'Q';
                    if (pos->board[square] == P_KING)
                        *p++ = 'K';
                }
                else
                {
                    if (pos->board[square] == P_PAWN)
                        *p++ = 'p';
                    if (pos->board[square] == P_ROOK)
                        *p++ = 'r';
                    if (pos->board[square] == P_KNIGHT)
                        *p++ = 'n';
                    if (pos->board[square] == P_BISHOP)
                        *p++ = 'b';
                    if (pos->board[square] == P_QUEEN)
                        *p++ = 'q';
                    if (pos->board[square] == P_KING)
                        *p++ = 'k';
                }
            }
        }
        if (empty > 0)
        {
            *p++ = '0' + empty;
            empty = 0;
        }
        if (rank > 0)
        {
            *p++ = '/';
        }
    }
    
    // side to move
    *p++ = ' ';
    if (pos->to_move == C_WHITE)
        *p++ = 'w';
    else
        *p++ = 'b';

    // castle rights
    *p++ = ' ';
    if (pos->state->castleRights == CASTLE_NONE)
        *p++ = '-';
    else
    {
        if (pos->state->castleRights & CASTLE_WHITE_OO)
            *p++ = 'K';
        if (pos->state->castleRights & CASTLE_WHITE_OOO)
            *p++ = 'Q';
        if (pos->state->castleRights & CASTLE_BLACK_OO)
            *p++ = 'k';
        if (pos->state->castleRights & CASTLE_BLACK_OOO)
            *p++ = 'q';
    }
    
    // en-passant
    *p++ = ' ';
    if (pos->state->enPassant != SQ_NONE)
    {
        *p++ = 'a' + FILE_OF(pos->state->enPassant);
        *p++ = '1' + RANK_OF(pos->state->enPassant);
    }
    else
    {
        *p++ = '-';
    }
        
    // half move clock
    p += sprintf(p, " %d", pos->state->rule50);
    
    // full move counter
    p += sprintf(p, " %d", 1 + (pos->state->plyNum / 2));
    
    return result;
}

void position_move(position_t* pos, move_t move)
{
    int from = MOVE_FROM(move);
    int to = MOVE_TO(move);
    color_t Cur = pos->to_move;
    color_t opp = Cur ^ 1;
    bitboard_t bft = BB(from) ^ BB(to);
    int piece = pos->board[from];
    int capture = MOVE_TYPE(move) == M_ENPASSANT ? P_PAWN : pos->board[to];
    state_t* state = pos->state + 1;
    pkey_t key = pos->state->key;
#if USE_PIECE_LIST
    int s;
#endif
    //int givesCheck = position_move_gives_check(pos, move);

    *state = *pos->state;
    state->prev = pos->state;
    pos->state = state;
    state->move = move;    
    state->rule50++;
    state->plyNum++;

    stats_inc(ST_NODE, 1);
    ASSERT_IF_FAIL(pos->state + 1000 > state, "exceeded array");
    
    if (state->castleRights & (CastleMaskFrom[from] | CastleMaskTo[to]))
    {
        int rights = CastleMaskFrom[from] | CastleMaskTo[to];
        key ^= Zob_castle[state->castleRights & rights];
        state->castleRights &= ~rights;
    }
    
    if (MOVE_TYPE(move) == M_CASTLE)
    {
        // move rook
        int s1=0, s2=0;
        if (to == C1)      { s1 = A1; s2 = D1; }
        else if (to == G1) { s1 = H1; s2 = F1; } 
        else if (to == C8) { s1 = A8; s2 = D8; }
        else if (to == G8) { s1 = H8; s2 = F8; }
        bitboard_t bft2 = BB(s1) | BB(s2); 

        pos->board[s2] = pos->board[s1];
        pos->board[s1] = P_NONE;
        pos->color[Cur] ^= bft2;
        pos->type[P_ROOK] ^= bft2;
        UPDATE_PLIST(Cur, P_ROOK, pos->index[s1], s2);
        state->score += SquareValue[P_ROOK][Cur][s2] - SquareValue[P_ROOK][Cur][s1];
        key ^= Zob_psq[Cur][P_ROOK][s1] ^ Zob_psq[Cur][P_ROOK][s2];
    }
    
    state->captured = capture;
    
    if (capture != P_NONE)
    {
        int cSquare = to;
        if (capture == P_PAWN)
        {
            if (MOVE_TYPE(move) == M_ENPASSANT)
            {
                cSquare += (Cur == C_WHITE ? -8 : 8);
                pos->board[cSquare] = P_NONE;
            }
        }
        else
        {
            state->npMaterial[opp] -= PieceValue[capture][PHASE_MG];
        }
        pos->color[opp] ^= BB(cSquare);
        pos->type[capture] ^= BB(cSquare);
#if USE_PIECE_LIST
        s = pos->piece[opp][capture][--pos->pCount[opp][capture]];
        UPDATE_PLIST(opp, capture, pos->index[cSquare], s);
        pos->piece[opp][capture][pos->pCount[opp][capture]] = SQ_NONE;
#endif
        state->score -= SquareValue[capture][opp][cSquare];
        key ^= Zob_psq[opp][capture][cSquare];
        state->rule50 = 0;
    }

    if (state->enPassant != SQ_NONE)
    {
        key ^= Zob_enpassant[FILE_OF(state->enPassant)];
        state->enPassant = SQ_NONE;
    }

    // move piece
    pos->color[Cur] ^= bft;
    pos->type[piece] ^= bft;
    pos->board[to] = pos->board[from];
    pos->board[from] = P_NONE;
#if USE_PIECE_LIST
    UPDATE_PLIST(Cur, piece, pos->index[from], to);
#endif
    state->score += SquareValue[piece][Cur][to] - SquareValue[piece][Cur][from];
    key ^= Zob_psq[Cur][piece][from] ^ Zob_psq[Cur][piece][to];
    
    if (piece == P_PAWN)
    {
        if ((from ^ to) == 16)
        {
            state->enPassant = (from + to) / 2;
            key ^= Zob_enpassant[FILE_OF(from)];
        }
        if (MOVE_TYPE(move) == M_PROMOTION)
        {
            int prom = MOVE_PROM(move);
            pos->type[P_PAWN] ^= BB(to);
            pos->type[prom] |= BB(to);
            pos->board[to] = prom;
            state->npMaterial[Cur] += PieceValue[prom][PHASE_MG];
#if USE_PIECE_LIST
            s = pos->piece[Cur][P_PAWN][--pos->pCount[Cur][P_PAWN]];
            UPDATE_PLIST(Cur, P_PAWN, pos->index[to], s);
            pos->piece[Cur][P_PAWN][pos->pCount[Cur][P_PAWN]] = SQ_NONE;
            UPDATE_PLIST(Cur, prom, pos->pCount[Cur][prom]++, to);
#endif
            state->score += SquareValue[prom][Cur][to] - SquareValue[P_PAWN][Cur][to];
            key ^= Zob_psq[Cur][P_PAWN][to] ^ Zob_psq[Cur][prom][to];
        }
        state->rule50 = 0;
    }
    
    pos->to_move = opp;
    key ^= Zob_side;
    //log_line("+side %llx", Zob_side);

    state->key = key;
    //position_print(pos);
    //if (givesCheck)
        state->checkers = position_checkers(pos);
    //else
    //    state->checkers = 0;
        
    //state->pinned = position_pinned(pos);
    //position_get_checkinfo(pos);
    
    ASSERT_IF_FAIL(position_compute_key(pos) == state->key, "key mismatch");
    ASSERT_IF_FAIL(position_compute_score(pos) == state->score, "score mismatch");
}

int position_unmove(position_t* pos)
{
    if (!pos->state->prev)
        return 0;
    
    move_t move = pos->state->move;
    int from = MOVE_FROM(move);
    int to = MOVE_TO(move);
    bitboard_t opp = pos->to_move;
    bitboard_t Cur = opp ^ 1;
    bitboard_t bft = BB(from) ^ BB(to);
    int piece = pos->board[to];
    int capture = pos->state->captured;
#if USE_PIECE_LIST
    int s;
#endif

    //move_print(move, "undo move ");

    if (MOVE_TYPE(move) == M_CASTLE)
    {
        int s1=0, s2=0;
        if (to == C1)      { s1 = A1; s2 = D1; }
        else if (to == G1) { s1 = H1; s2 = F1; } 
        else if (to == C8) { s1 = A8; s2 = D8; }
        else if (to == G8) { s1 = H8; s2 = F8; }
        bitboard_t bft2 = BB(s1) | BB(s2); 

        pos->board[s1] = pos->board[s2];
        pos->board[s2] = P_NONE;
        pos->color[Cur] ^= bft2;
        pos->type[P_ROOK] ^= bft2;
        UPDATE_PLIST(Cur, P_ROOK, pos->index[s2], s1);
    }
    else if (MOVE_TYPE(move) == M_PROMOTION)
    {
        int prom = MOVE_PROM(move);
        pos->type[prom] ^= BB(to);
        pos->type[P_PAWN] |= BB(to);
        pos->board[to] = P_PAWN;
        piece = P_PAWN;
        bft = BB(from) ^ BB(to);
#if USE_PIECE_LIST
        s = pos->piece[Cur][prom][--pos->pCount[Cur][prom]];
        UPDATE_PLIST(Cur, prom, pos->index[to], s);
        pos->piece[Cur][prom][pos->pCount[Cur][prom]] = SQ_NONE;
        UPDATE_PLIST(Cur, P_PAWN, pos->pCount[Cur][P_PAWN]++, to);
#endif
    }
    
    // unmove piece
    pos->color[Cur] ^= bft;
    pos->type[piece] ^= bft;
    pos->board[from] = pos->board[to];
    pos->board[to] = P_NONE;
    UPDATE_PLIST(Cur, piece, pos->index[to], from);
    
    if (capture != P_NONE)
    {
        int cSquare = to;
        if (MOVE_TYPE(move) == M_ENPASSANT)
            cSquare += (Cur == C_WHITE ? -8 : 8);
        pos->color[opp] |= BB(cSquare);
        pos->type[capture] |= BB(cSquare);
        pos->board[cSquare] = capture;
#if USE_PIECE_LIST
        UPDATE_PLIST(opp, capture, pos->pCount[opp][capture]++, cSquare);
#endif
    }

    pos->to_move = Cur;
    pos->state = pos->state->prev;
    
    return 1;
}


int position_king_square(position_t* pos, color_t color)
{
#if USE_PIECE_LIST
    return pos->piece[color][P_KING][0];
#else
    return GLSB(pos->type[P_KING] & pos->color[color]);
#endif
}

/**
 * Get all straight sliding pieces of a color.
 * @param position the position
 * @param color the color of interest
 * @return bitboard with all requested pieces
 */
bitboard_t position_rook_pieces(position_t* pos, color_t color)
{
    return pos->color[color] & (pos->type[P_ROOK] | pos->type[P_QUEEN]);
}

/**
 * Get all diagonal sliding pieces of a color.
 * @param position the position
 * @param color the color of interest
 * @return bitboard with all requested pieces
 */
bitboard_t position_bishop_pieces(position_t* pos, color_t color)
{
    return pos->color[color] & (pos->type[P_BISHOP] | pos->type[P_QUEEN]);
}

/**
 * Get all pieces that are pinned against the king.
 * @param position the position
 * @return bitboard of all pinned pieces
 */
bitboard_t position_pinned(position_t* pos)
{
    color_t Cur = pos->to_move;
    color_t opp = 1 ^ Cur;
    bitboard_t myPieces = pos->color[Cur];
    bitboard_t allPieces = pos->color[C_WHITE] | pos->color[C_BLACK];
    int ksq = position_king_square(pos, Cur);
    // get all enemy sliding pieces that can reach king on empty board
    bitboard_t pinners =
        (position_rook_pieces(pos, opp) & B_RookAttacks[ksq]) |
        (position_bishop_pieces(pos, opp) & B_BishopAttacks[ksq]);
    bitboard_t result = 0;

    // get pieces between their pinners and king (if there is only one between)
    while (pinners)
    {
        int index;
        PLSB(index, pinners);
        bitboard_t b = B_Between[ksq][index] & allPieces;
        if (!BC_MORE_THAN_ONE(b) && (b & myPieces))
            result |= b;
    }
    return result;
}

/**
 * Get all pieces that give check if they are move out of the way.
 * @param position the position
 * @return bitboard of all hidden checkers
 */
bitboard_t position_hidden_checkers(position_t* pos)
{
    color_t cur = pos->to_move;
    color_t opp = 1 ^ cur;
    bitboard_t myPieces = pos->color[cur];
    bitboard_t allPieces = pos->color[C_WHITE] | pos->color[C_BLACK];
    int ksq = position_king_square(pos, opp);
    // get sliding pieces that can reach the enemy king on empty board
    bitboard_t pinners =
        (position_rook_pieces(pos, cur) & B_RookAttacks[ksq]) |
        (position_bishop_pieces(pos, cur) & B_BishopAttacks[ksq]);
    bitboard_t result = 0;

    // get pieces between pinners and their king (if there is only one between)
    while (pinners)
    {
        int index;
        PLSB(index, pinners);
        bitboard_t b = B_Between[ksq][index] & allPieces;
        if (!BC_MORE_THAN_ONE(b) && (b & myPieces))
            result |= b;
    }
    return result;
}

/**
 * Return squares that are attacked by the piece on a given square.
 * @param position the position
 * @param square the square with the attacking piece
 * @return bitboard with all attacked squares
 */
bitboard_t position_attacks_from(position_t* pos, int square, int piece, bitboard_t occ)
{
    if (piece == P_KING)
        return B_KingAttacks[square];
    if (piece == P_QUEEN)
        return sliding_attack_rook(square, occ) | sliding_attack_bishop(square, occ);
    if (piece == P_ROOK)
        return sliding_attack_rook(square, occ);
    if (piece == P_BISHOP)
        return sliding_attack_bishop(square, occ);
    if (piece == P_KNIGHT)
        return B_KnightAttacks[square];
    if (piece == P_PAWN)
    {
        if (pos->color[C_WHITE] & BB(square))
           return B_WPawnAttacks[square];
        else
           return B_BPawnAttacks[square];
    }
    return 0;
}

/**
  * Return all pieces on board which attack a given square.
  * @param position the position
  * @param square the square to check
  * @return the bitboard with all attacking pieces.
 */
bitboard_t position_attackers_to(position_t* pos, int square)
{
    bitboard_t occ = pos->color[C_WHITE] | pos->color[C_BLACK];
    return position_attackers_to_ext(pos, square, occ);
}

bitboard_t position_attackers_to_ext(position_t* pos, int square, bitboard_t occ)
{
    return (
     (B_KingAttacks[square] & pos->type[P_KING]) |
     (sliding_attack_rook(square, occ) & (pos->type[P_ROOK] | pos->type[P_QUEEN])) |
     (sliding_attack_bishop(square, occ) & (pos->type[P_BISHOP] | pos->type[P_QUEEN])) |
     (B_KnightAttacks[square] & pos->type[P_KNIGHT]) |
     (B_WPawnAttacks[square] & pos->color[C_BLACK] & pos->type[P_PAWN]) |
     (B_BPawnAttacks[square] & pos->color[C_WHITE] & pos->type[P_PAWN])
            );
}

bitboard_t position_checkers(position_t* pos)
{
    int kingSquare = position_king_square(pos, pos->to_move);
    return pos->color[1 ^ pos->to_move] & position_attackers_to(pos, kingSquare);
}



void position_get_checkinfo(position_t* pos)
{
    stats_inc(ST_PINNED, 1);
    pos->state->pinned = position_pinned(pos);
    //pos->state->hCheckers = position_hidden_checkers(pos);
}
    
int position_move_gives_check(position_t* pos, move_t move)
{
    int from = MOVE_FROM(move);
    int to = MOVE_TO(move);
    int piece = MOVE_TYPE(move) == M_PROMOTION ? MOVE_PROM(move) : pos->board[from];
    int cur = pos->to_move;
    int opp = 1 ^ cur;
    int square = position_king_square(pos, opp);
    bitboard_t occ = (pos->color[C_WHITE] | pos->color[C_BLACK] | BB(to)) ^ BB(from);

    ASSERT_IF_FAIL(pos->state->hCheckers == position_hidden_checkers(pos), "hidden checkers mismatch");
    
    // check if the the enemy king is directly attacked from "to"
    // note: do the check from the king square to get the right pawn direction.
    if (position_attacks_from(pos, square, piece, occ) && BB(to))
        return 1;

    // check if moving piece is a hidden checker.
    if (pos->state->hCheckers && (pos->state->hCheckers & BB(from)))
    {
        // knights always discover check
        // bishops, rooks, queens either give direct check, or discover check
        // for king and pawn, check if they leave the line.
        // FIXME: really need to test the KING?
        if ((piece != P_PAWN && piece != P_KING) ||
            !squares_aligned(from, to, square))
          return 1;
    }

    if (MOVE_TYPE(move) == M_NORMAL)
        return 0;
        
    switch (MOVE_TYPE(move))
    {
    case M_PROMOTION:
        return 0;
    case M_ENPASSANT:
        // handle the rare case where the two pawns discover a check
        occ ^= BB(SQ(FILE_OF(to), RANK_OF(from)));
        return (sliding_attack_rook(square, occ) & position_rook_pieces(pos, cur)) |
               (sliding_attack_bishop(square, occ) & position_bishop_pieces(pos, cur));
    case M_CASTLE:
    {
        int s1=0, s2=0;
        if (to == C1)      { s1 = A1; s2 = D1; }
        else if (to == G1) { s1 = H1; s2 = F1; } 
        else if (to == C8) { s1 = A8; s2 = D8; }
        else if (to == G8) { s1 = H8; s2 = F8; }
        occ ^= BB(s1) | BB(s2);
        return sliding_attack_rook(square, occ) & BB(s2);
    }
    default:
        ASSERT_IF_FAIL(0, "unreachable");
        return 0;
    }
}

int position_in_check(position_t* pos)
{
    return pos->state->checkers != 0;
}

int position_is_draw(position_t* pos, int checkRepetition, int check3Fold)
{
    state_t* state = pos->state;
    
    if (pos->type[P_PAWN] == 0 &&
        state->npMaterial[C_WHITE] + state->npMaterial[C_BLACK] <= PieceValue[P_BISHOP][PHASE_MG])
    {
        return 1;
    }
        
    if (pos->state->rule50 > 99)
    {
        move_stack_t moves[MAX_MOVES];
        if (!position_in_check(pos) || move_generate_legal(pos, moves) == moves)
        {
            return 1;
        }
    }
    
    if (checkRepetition)
    {
        int cnt = 0;
        int num = state->rule50;
        state_t* pstate = state;
        
        while (num >= 2 && pstate->prev)
        {
            pstate = pstate->prev->prev;
            if (!pstate)
                break;
            if (pstate->key == state->key)
            {
                if (!check3Fold || ++cnt >= 2)
                {
//                    log_line("...draw");
                    return 1;
                }
            }
        }
    }
    return 0;
}

int position_pseudo_is_legal(position_t* pos, move_t move)
{
    int Cur = pos->to_move;
    int from = MOVE_FROM(move);

    ASSERT_IF_FAIL(pos->state->pinned == position_pinned(pos), "pinned mismatch");
    
    if (MOVE_TYPE(move) == M_ENPASSANT)
    {
        int opp = 1 ^ Cur;
        int to = MOVE_TO(move);
        int capSquare = to + (Cur == C_WHITE ? DELTA_S : DELTA_N);
        int kingSquare = position_king_square(pos, Cur);
        bitboard_t allPieces = pos->color[C_WHITE] | pos->color[C_BLACK];
        bitboard_t occ = (allPieces ^ (BB(from) | BB(capSquare))) | BB(to);
        return !(sliding_attack_rook(kingSquare, occ) & position_rook_pieces(pos, opp)) &&
               !(sliding_attack_bishop(kingSquare, occ) & position_bishop_pieces(pos, opp));
    }
    
    if (pos->board[from] == P_KING)
    {
        return MOVE_TYPE(move) == M_CASTLE ||
            !(position_attackers_to(pos, MOVE_TO(move)) & pos->color[1 ^ Cur]);
    }

    return
        !pos->state->pinned ||
        !(pos->state->pinned & BB(from)) ||
        squares_aligned(from, MOVE_TO(move), position_king_square(pos, Cur));
}

int position_move_is_pseudo(position_t* pos, move_t move)
{
    color_t cur = pos->to_move;
    color_t opp = 1 ^ cur;
    int from = MOVE_FROM(move);
    int to = MOVE_TO(move);
    int piece = pos->board[from];
    color_t fcolor = (pos->color[cur] & BB(from)) ? cur :
                     (pos->color[opp] & BB(from)) ? opp : C_NONE;
    color_t tcolor = (pos->color[cur] & BB(to)) ? cur :
                     (pos->color[opp] & BB(to)) ? opp : C_NONE;
    bitboard_t occ = pos->color[C_WHITE] | pos->color[C_BLACK];
    
    if (MOVE_TYPE(move) != M_NORMAL)
        return position_move_is_legal(pos, move);

    if (MOVE_PROM(move) - P_KNIGHT != P_NONE)
        return 0;

    if (piece == P_NONE || fcolor != cur)
        return 0;

    if (tcolor == cur)
        return 0;

    if (piece == P_PAWN)
    {
        int dir = to - from;
        if ((cur == C_WHITE) != (dir > 0))
            return 0;

        if (RANK_OF(to) == Rank8 || RANK_OF(to) == Rank1)
            return 0;

        switch (dir)
        {
        case DELTA_NW:
        case DELTA_NE:
        case DELTA_SW:
        case DELTA_SE:
            if (tcolor != opp)
                return 0;
            if (abs(FILE_OF(from) - FILE_OF(to)) != 1)
                return 0;
            break;
        case DELTA_N:
        case DELTA_S:
            if (tcolor != C_NONE)
                return 0;
            break;
        case DELTA_NN:
            if (RANK_OF(to) != Rank4 ||
                tcolor != C_NONE ||
                (BB(from + DELTA_N) & occ))
                return 0;
            break;
        case DELTA_SS:
            if (RANK_OF(to) != Rank5 ||
                tcolor != C_NONE ||
                (BB(from + DELTA_S) & occ))
                return 0;
            break;
        default:
            return 0;
        }
    }
    else if (!(position_attacks_from(pos, from, piece, occ) & BB(to)))
    {
        return 0;
    }

    if (position_in_check(pos))
    {
        if (piece != P_KING)
        {
            bitboard_t b = pos->state->checkers;
            int square;
            PLSB(square, b);
            
            if (b)
                return 0;
            b = pos->state->checkers;
            
            if (to != square &&
                (B_Between[square][position_king_square(pos, cur)] & BB(to)) == 0)
                return 0;
        }
        else if (position_attackers_to_ext(pos, to, occ ^ BB(from)) & pos->color[opp])
        {
            return 0;
        }
    }
    return 1;
}

int position_move_is_legal(position_t* pos, move_t move)
{
    move_stack_t moves[MAX_MOVES];
    move_stack_t* last = move_generate_legal(pos, moves);
    move_stack_t* cur = moves;
    
    while (cur != last)
    {
        if (cur->move == move)
            return 1;
        cur++;
    }
    return 0;
}

int position_see_sign(position_t* pos, move_t move)
{
    if (PieceValue[pos->board[MOVE_TO(move)]][PHASE_MG] >=
        PieceValue[pos->board[MOVE_FROM(move)]][PHASE_MG])
        return 1;
        
    return position_see(pos, move);
}

int position_see(position_t* pos, move_t move)
{
    int from = MOVE_FROM(move);
    int to = MOVE_TO(move);
    bitboard_t occ = (pos->color[C_WHITE] | pos->color[C_BLACK]) ^ BB(from);
    int captured = pos->board[to];
  
    if (MOVE_TYPE(move) == M_ENPASSANT)
    {
        int capSq = to - (pos->to_move == C_WHITE ? DELTA_N : DELTA_S);
        occ ^= BB(capSq);
        captured = P_PAWN;
    }
    else if (MOVE_TYPE(move) == M_CASTLE)
    {
        return 0;
    }
    
    if (captured == P_NONE)
        return 0;
        
    bitboard_t attackers = position_attackers_to_ext(pos, to, occ);

    color_t opp = (pos->color[C_WHITE] & BB(from)) ? C_BLACK : C_WHITE;
    bitboard_t oppAttackers = attackers & pos->color[opp];

    if (!oppAttackers)
        return PieceValue[captured][PHASE_MG];

    int swap[32];
    int index = 0;
    swap[index++] = PieceValue[captured][PHASE_MG];
    captured = pos->board[from];

    do
    {
        ASSERT_IF_FAIL(index < 32, "index exceeds 32");

        swap[index] = -swap[index-1] + PieceValue[captured][PHASE_MG];
        index++;

        // find least valuable attacker
        int piece;
        for (piece = P_PAWN; piece <= P_KING; piece++)
        {
            bitboard_t b;
            if ((b = (oppAttackers & pos->type[piece])) != 0)
            {
                occ ^= b & -b;
                
                if (piece == P_PAWN || piece == P_BISHOP || piece == P_QUEEN)
                    attackers |= sliding_attack_bishop(to, occ) & (pos->type[P_BISHOP] | pos->type[P_QUEEN]);

                if (piece == P_ROOK || piece == P_QUEEN)
                    attackers |= sliding_attack_rook(to, occ) & (pos->type[P_ROOK] | pos->type[P_QUEEN]);
                
                captured = piece;
                break;
            }
        }
        
        attackers &= occ;
        opp = 1 ^ opp;
        oppAttackers = attackers & pos->color[opp];
        
        if (captured == P_KING)
        {
            if (oppAttackers)
                swap[index++] = PieceValue[P_QUEEN][PHASE_MG] * 16;
            break;
        }
    } while (oppAttackers);

    while (--index)
    {
        if (-swap[index] < swap[index-1])
            swap[index-1] = -swap[index];
    }

    return swap[0];
}

static pkey_t position_compute_key(position_t* pos)
{
    state_t* state = pos->state;
    pkey_t k = Zob_castle[state->castleRights];
    //log_line("  compute %llx", k);
    
    int square;
    for (square = A1; square <= H8; square++)
    {
        if (pos->board[square] != P_NONE)
        {
            color_t color = pos->color[C_WHITE] & BB(square) ? C_WHITE : C_BLACK;
            k ^= Zob_psq[color][pos->board[square]][square];
            //log_line("  piece %16llx  %2d %2d %2d",
            //    Zob_psq[color][pos->board[square]][square],
            //    color, square, pos->board[square]);
        }
    }
    if (state->enPassant != SQ_NONE)
    {
        k ^= Zob_enpassant[FILE_OF(state->enPassant)];
        //log_line("  enp %llx", Zob_enpassant[FILE_OF(state->enPassant)]);
    }
    if (pos->to_move == C_BLACK)
    {
        k ^= Zob_side;
        //log_line("  side %llx", Zob_side);
    }
        
    return k;
}

static int position_compute_score(position_t* pos)
{
    int score = 0;
    int square;

    for (square = A1; square <= H8; square++)
    {
        int piece = pos->board[square];
        if (piece != P_NONE)
        {
            color_t color = pos->color[C_WHITE] & BB(square) ? C_WHITE : C_BLACK;
            score += SquareValue[piece][color][square];
        }
    }
    return score;
}

int position_is_cap_or_prom(position_t* pos, move_t move)
{
    return
        MOVE_TYPE(move) == M_PROMOTION ||
        MOVE_TYPE(move) == M_ENPASSANT ||
        pos->board[MOVE_TO(move)] != 0;
}

int position_is_cap(position_t* pos, move_t move)
{
    return
        MOVE_TYPE(move) == M_ENPASSANT ||
        pos->board[MOVE_TO(move)] != 0;
}

void position_check_vars(position_t* pos)
{
    int score = position_compute_score(pos);
    if (score == pos->state->score)
    {
        log_line("Score = (%d, %d)", MG_VAL(score), EG_VAL(score));
    }
    else
    {
        log_error("Score mismatch");
        log_error("    is (%d, %d)", MG_VAL(pos->state->score), EG_VAL(pos->state->score));
        log_error("  real (%d, %d)", MG_VAL(score), EG_VAL(score));
    }
    
    pkey_t key = position_compute_key(pos);
    if (key == pos->state->key)
    {
        log_line("Key = %llx", key);
    }
    else
    {
        log_error("Key mismatch");
        log_error("     is %llx", key);
        log_error("   real %llx", pos->state->key);
    }
    
    
#if USE_PIECE_LIST
    color_t c;
    int type, i1;
    for (c = C_WHITE; c <= C_BLACK; c++)
        for (type = P_PAWN; type <= P_KING; type++)
        {
            int cnt = 0;
            bitboard_t pieces = pos->color[c] & pos->type[type];
            while (pieces)
            {
                int index;
                PLSB(index, pieces);
                cnt++;
            }
            if (pos->pCount[c][type] != cnt)
            {
                log_error("Piece list count mismatch %d %d", c, type);
                log_error("     is %d", pos->pCount[c][type]);
                log_error("   real %d", cnt);
                return;
            }
        }

    for (c = C_WHITE; c <= C_BLACK; c++)
        for (type = P_PAWN; type <= P_KING; type++)
            for (i1 = 0; i1 < pos->pCount[c][type]; i1++)
            {
                int square = pos->piece[c][type][i1];
                if (pos->board[square] != type ||
                    (pos->type[type] & BB(square)) == 0 ||
                    (pos->color[c] & BB(square)) == 0)
                {
                    log_error("Piece square mismatch %d %d [%d] on %d", c, type, i1, square);
                    log_error("     is type %d", type);
                    log_error("   real type %d", pos->board[square]);
                    return;
                }
                if (pos->index[square] != i1)
                {
                    log_error("Piece index mismatch %d %d [%d] on %d", c, type, i1, square);
                    log_error("     is type %d", pos->index[square]);
                    log_error("   real type %d", i1);
                    return;
                }
            }
#endif
}

int position_pawns_on7(position_t* pos, color_t color)
{
    bitboard_t rank7 = color == C_WHITE ? B_Rank7 : B_Rank2;
    return (pos->type[P_PAWN] & pos->color[color] & rank7) != 0;
}



