#include "position.h"
#include "stats.h"
#include "log.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "squaretable.h"
#include "move.h"
#include "prnd.h"

const char P_CHAR[PieceNum] = { ' ', 'p', 'n', 'b', 'r', 'q', 'k' };
const int PieceValue[PieceNum][PHASE_NUM] = {
    {3500, 3500},
    {198,  258 },
    {817,  846 },
    {836,  857 },
    {1270, 1278},
    {2521, 2558},
};
const int PieceValue2[PieceNum][PHASE_NUM] = {
    {1200, 1200},
    {100,  100 },
    {250,  250 },
    {300,  300 },
    {550,  550 },
    {1000, 1000},
};
int SquareValue[PieceNum][ColorNum][SquareNum];

int CastleMaskFrom[SquareNum];
int CastleMaskTo[SquareNum];

pkey_t Zob_psq[ColorNum][PieceNum][SquareNum];
pkey_t Zob_enpassant[FileNum];
pkey_t Zob_castle[CASTLING_ALL + 1];
pkey_t Zob_side;
//pkey_t Zob_exclusion;

static pkey_t position_compute_key(position_t* pos);
static int position_compute_score(position_t* pos);


void positions_init()
{
  int piece;
  int square;
  int file;
  int cr;

  prnd_init();

  for (auto color = (int)White; color <= (int)Black; color++)
    for (piece = Pawn; piece <= King; piece++)
      for (square = A1; square <= H8; square++)
        Zob_psq[color][piece][square] = prand64();
  for (file = FileA; file <= FileH; file++)
    Zob_enpassant[file] = prand64();

  for (cr = CASTLING_NONE; cr <= CASTLING_ALL; cr++)
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

  for (piece = Pawn; piece <= King; piece++)
  {
    int baseScore = MAKE_SCORE(PieceValue[piece][PHASE_MG], PieceValue[piece][PHASE_EG]);
    int square;

    for (square = A1; square <= H8; square++)
    {
      int addScore = MAKE_SCORE(PST[piece][PHASE_MG][square], PST[piece][PHASE_EG][square]);
      //addScore = 0;
      SquareValue[piece][White][square] = (baseScore + addScore);
      SquareValue[piece][Black][63 - square] = -(baseScore + addScore);
    }
  }

  memset(CastleMaskFrom, 0, SquareNum * sizeof(*CastleMaskFrom));
  memset(CastleMaskTo, 0, SquareNum * sizeof(*CastleMaskTo));
  CastleMaskFrom[E1] = WHITE_OO | WHITE_OOO;
  CastleMaskFrom[E8] = BLACK_OO | BLACK_OOO;
  CastleMaskFrom[A1] = WHITE_OOO;
  CastleMaskTo[A1] = WHITE_OOO;
  CastleMaskFrom[A8] = BLACK_OOO;
  CastleMaskTo[A8] = BLACK_OOO;
  CastleMaskFrom[H1] = WHITE_OO;
  CastleMaskTo[H1] = WHITE_OO;
  CastleMaskFrom[H8] = BLACK_OO;
  CastleMaskTo[H8] = BLACK_OO;
}

char* move_format(Move move)
{
  static char result[100];
  int from = from_sq(move);
  int to = to_sq(move);

  if (type_of(move) == Promotion)
    sprintf(result, "%c%c%c%c%c",
      'a' + (from % 8), '1' + (from / 8),
      'a' + (to % 8), '1' + (to / 8),
      P_CHAR[promotionPiece(move)]);
  else
    sprintf(result, "%c%c%c%c",
      'a' + (from % 8), '1' + (from / 8),
      'a' + (to % 8), '1' + (to / 8));
  return result;
}

position_t* position_new(void)
{
  position_t* pos = new position_t;
  position_reset(pos);
  return pos;
}

static void position_clear(position_t* pos)
{
  memset(pos, 0, sizeof(*pos));
  pos->to_move = ColorNone;
  pos->state = pos->states;
  pos->state->enPassant = SquareNone;

#if USE_PIECE_LIST
  {
    int i1, i2, i3;

    for (i1 = 0; i1 < ColorNum; i1++)
      for (i2 = 0; i2 < PieceNum; i2++)
        for (i3 = 0; i3 < 16; i3++)
          pos->piece[i1][i2][i3] = SquareNone;
  }
#endif
}

#if USE_PIECE_LIST
#  define UPDATE_PLIST(col, type, i, to) \
    pos->piece[col][type][pos->index[to] = i] = to;
#else
#  define UPDATE_PLIST(Cur, type, i, to)
#endif

static void position_add_piece(position_t* pos, Square square, PieceType piece, Color color)
{
  bitboard_t bs = BB(square);

  pos->color[color] |= bs;
  pos->type[piece] |= bs;
  pos->board[square] = piece;
  UPDATE_PLIST(color, piece, pos->pCount[color][piece]++, square);
  pos->state->score += SquareValue[piece][color][square];
  if (piece != Pawn)
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

void position_print(position_t* pos, Color color)
{
  FILE* fd = log_get_fd();

  if (!fd)
    return;

  if (color == White)
  {
    fprintf(fd, "\033[40m                                         \033[0m\n");
    for (auto rank = Rank8; rank >= Rank1; --rank)
    {
      fprintf(fd, "\033[40m   ");
      fprintf(fd, "\033[47m +---+---+---+---+---+---+---+---+ ");
      fprintf(fd, "\033[40m   \033[0m\n");
      fprintf(fd, "\033[1;37;40m %d ", rank + 1);
      fprintf(fd, "\033[47m");
      for (auto file = FileA; file <= FileH; ++file)
      {
        auto square = make_square(file, rank);
        bitboard_t mask = BB(square);
        fprintf(fd, "\033[1;30m |");
        if (pos->color[Black] & mask)
          fprintf(fd, "\033[1;30m");
        else
          fprintf(fd, "\033[1;31m");

        if (pos->type[Pawn] & mask)
          fprintf(fd, " P");
        else if (pos->type[Rook] & mask)
          fprintf(fd, " R");
        else if (pos->type[Knight] & mask)
          fprintf(fd, " N");
        else if (pos->type[Bishop] & mask)
          fprintf(fd, " B");
        else if (pos->type[Queen] & mask)
          fprintf(fd, " Q");
        else if (pos->type[King] & mask)
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
  else if (color == Black)
  {
    fprintf(fd, "\033[40m                                         \033[0m\n");
    for (auto rank = Rank1; rank <= Rank8; ++rank)
    {
      fprintf(fd, "\033[40m   ");
      fprintf(fd, "\033[47m +---+---+---+---+---+---+---+---+ ");
      fprintf(fd, "\033[40m   \033[0m\n");
      fprintf(fd, "\033[1;37;40m %d ", rank + 1);
      fprintf(fd, "\033[47m");
      for (auto file = FileH; file >= FileA; --file)
      {
        auto square = make_square(file, rank);
        bitboard_t mask = BB(square);
        fprintf(fd, "\033[1;30m |");
        if (pos->color[Black] & mask)
          fprintf(fd, "\033[1;30m");
        else
          fprintf(fd, "\033[1;31m");

        if (pos->type[Pawn] & mask)
          fprintf(fd, " P");
        else if (pos->type[Rook] & mask)
          fprintf(fd, " R");
        else if (pos->type[Knight] & mask)
          fprintf(fd, " N");
        else if (pos->type[Bishop] & mask)
          fprintf(fd, " B");
        else if (pos->type[Queen] & mask)
          fprintf(fd, " Q");
        else if (pos->type[King] & mask)
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
  log_line("Invalid FEN string at position %d", index + 1);
  log_line("%s", fen);
  log_line("%*c", index + 1, '^');
  log_line("%*c%s", index, ' ', error);
  log_line("");
}

int position_set(position_t* position, const char* fen)
{
  const char* pos = fen;
  Rank rank = Rank8;
  File file = FileA;
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
        printFenError(fen, pos - fen, "Invalid number of empty squares");
        return 0;
      }
      else if (last_was_empty)
      {
        printFenError(fen, pos - fen, "Empty squares must not follow empty squares");
        return 0;
      }
      last_was_empty = 1;
      file += empty;
    }
    else if (*pos == '/')
    {
      if (file != 8)
      {
        printFenError(fen, pos - fen, "Unexpected start of new rank");
        return 0;
      }
      else if (rank == 0)
      {
        printFenError(fen, pos - fen, "No more rank expected");
        return 0;
      }
      file = FileA;
      --rank;
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
        position_add_piece(position, make_square(file, rank), Pawn, Black);
      else if (*pos == 'P')
        position_add_piece(position, make_square(file, rank), Pawn, White);
      else if (*pos == 'r')
        position_add_piece(position, make_square(file, rank), Rook, Black);
      else if (*pos == 'R')
        position_add_piece(position, make_square(file, rank), Rook, White);
      else if (*pos == 'n')
        position_add_piece(position, make_square(file, rank), Knight, Black);
      else if (*pos == 'N')
        position_add_piece(position, make_square(file, rank), Knight, White);
      else if (*pos == 'b')
        position_add_piece(position, make_square(file, rank), Bishop, Black);
      else if (*pos == 'B')
        position_add_piece(position, make_square(file, rank), Bishop, White);
      else if (*pos == 'q')
        position_add_piece(position, make_square(file, rank), Queen, Black);
      else if (*pos == 'Q')
        position_add_piece(position, make_square(file, rank), Queen, White);
      else if (*pos == 'k')
        position_add_piece(position, make_square(file, rank), King, Black);
      else if (*pos == 'K')
        position_add_piece(position, make_square(file, rank), King, White);
      else
      {
        printFenError(fen, pos - fen, "Invalid character");
        return 0;
      }
      ++file;
      last_was_empty = 0;
    }
    else
    {
      printFenError(fen, pos - fen, "Invalid character at end of file");
      return 0;
    }
    pos++;
  }
  if (file != 8 || rank != 0)
  {
    printFenError(fen, pos - fen, "Unexpected end of piece placement");
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
    else if (position->to_move == ColorNone)
    {
      if (*pos == 'w')
        position->to_move = White;
      else if (*pos == 'b')
      {
        position->to_move = Black;
        position->state->key ^= Zob_side;
      }
      else
      {
        printFenError(fen, pos - fen, "Invalid side to move");
        return 0;
      }
    }
    else
    {
      printFenError(fen, pos - fen, "Unexpected character for side to move");
      return 0;
    }
    pos++;
  }
  if (position->to_move == ColorNone)
  {
    printFenError(fen, pos - fen, "No side to move");
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
      printFenError(fen, pos - fen, "Unexpected character for castle rights");
      return 0;
    }
    else if (*pos == '-')
    {
      dash = 1;
    }
    else if (*pos == 'K')
    {
      position->state->castleRights |= WHITE_OO;
    }
    else if (*pos == 'Q')
    {
      position->state->castleRights |= WHITE_OOO;
    }
    else if (*pos == 'k')
    {
      position->state->castleRights |= BLACK_OO;
    }
    else if (*pos == 'q')
    {
      position->state->castleRights |= BLACK_OOO;
    }
    else
    {
      printFenError(fen, pos - fen, "Unexpected character for castle rights");
      return 0;
    }
    pos++;
  }
  if (!dash && position->state->castleRights == CASTLING_NONE)
  {
    printFenError(fen, pos - fen, "Invalid castle rights");
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
      printFenError(fen, pos - fen, "Unexpected character for en-passant");
      return 0;
    }
    else if (*pos == '-')
    {
      dash = 1;
    }
    else if ('a' <= *pos && *pos <= 'h' && (pos[1] == '3' || pos[1] == '6'))
    {
      File file = File(pos[0] - 'a');
      Rank rank = Rank(pos[1] - '1');
      position->state->enPassant = make_square(file, rank);
      position->state->key = Zob_enpassant[file];
      pos++;
    }
    else
    {
      printFenError(fen, pos - fen, "Unexpected square for en-passant");
      return 0;
    }
    pos++;
  }
  if (!dash && position->state->enPassant == SquareNone)
  {
    printFenError(fen, pos - fen, "Invalid en-passant");
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
      printFenError(fen, pos - fen, "Invalid character for halfmove clock");
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
      printFenError(fen, pos - fen, "Invalid character for fullmove counter");
      return 0;
    }
    else if (*pos == '0')
    {
      position->state->plyNum *= 10;
    }
    else
    {
      printFenError(fen, pos - fen, "Unexpected character for fullmove counter");
      return 0;
    }
    pos++;
  }
  if (position->state->plyNum)
  {
    position->state->plyNum *= 2;
    if (position->to_move == White)
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

  // piece placement
  for (auto rank = Rank7; rank >= Rank1; --rank)
  {
    int empty = 0;
    for (auto file = FileA; file <= FileH; ++file)
    {
      int square = make_square(file, rank);
      if (pos->board[square] == PieceNone)
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
        if (pos->color[White] & BB(square))
        {
          if (pos->board[square] == Pawn)
            *p++ = 'P';
          if (pos->board[square] == Rook)
            *p++ = 'R';
          if (pos->board[square] == Knight)
            *p++ = 'N';
          if (pos->board[square] == Bishop)
            *p++ = 'B';
          if (pos->board[square] == Queen)
            *p++ = 'Q';
          if (pos->board[square] == King)
            *p++ = 'K';
        }
        else
        {
          if (pos->board[square] == Pawn)
            *p++ = 'p';
          if (pos->board[square] == Rook)
            *p++ = 'r';
          if (pos->board[square] == Knight)
            *p++ = 'n';
          if (pos->board[square] == Bishop)
            *p++ = 'b';
          if (pos->board[square] == Queen)
            *p++ = 'q';
          if (pos->board[square] == King)
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
  if (pos->to_move == White)
    *p++ = 'w';
  else
    *p++ = 'b';

  // castle rights
  *p++ = ' ';
  if (pos->state->castleRights == CASTLING_NONE)
    *p++ = '-';
  else
  {
    if (pos->state->castleRights & WHITE_OO)
      *p++ = 'K';
    if (pos->state->castleRights & WHITE_OOO)
      *p++ = 'Q';
    if (pos->state->castleRights & BLACK_OO)
      *p++ = 'k';
    if (pos->state->castleRights & BLACK_OOO)
      *p++ = 'q';
  }

  // en-passant
  *p++ = ' ';
  if (pos->state->enPassant != SquareNone)
  {
    *p++ = 'a' + file_of(pos->state->enPassant);
    *p++ = '1' + rank_of(pos->state->enPassant);
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

void position_move(position_t* pos, Move move)
{
  Square from = from_sq(move);
  Square to = to_sq(move);
  Color Cur = pos->to_move;
  Color opp = ~Cur;
  bitboard_t bft = BB(from) ^ BB(to);
  PieceType piece = pos->board[from];
  PieceType capture = type_of(move) == EnPassant ? Pawn : pos->board[to];
  state_t* state = pos->state + 1;
  pkey_t key = pos->state->key;
  //int givesCheck = position_move_gives_check(pos, move);

  * state = *pos->state;
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

  if (type_of(move) == Castle)
  {
    // move rook
    Square s1 = SquareNone;
    Square s2 = SquareNone;
    if (to == C1) { s1 = A1; s2 = D1; }
    else if (to == G1) { s1 = H1; s2 = F1; }
    else if (to == C8) { s1 = A8; s2 = D8; }
    else if (to == G8) { s1 = H8; s2 = F8; }
    bitboard_t bft2 = BB(s1) | BB(s2);

    pos->board[s2] = pos->board[s1];
    pos->board[s1] = PieceNone;
    pos->color[Cur] ^= bft2;
    pos->type[Rook] ^= bft2;
    UPDATE_PLIST(Cur, Rook, pos->index[s1], s2);
    state->score += SquareValue[Rook][Cur][s2] - SquareValue[Rook][Cur][s1];
    key ^= Zob_psq[Cur][Rook][s1] ^ Zob_psq[Cur][Rook][s2];
  }

  state->captured = capture;

  if (capture != PieceNone)
  {
    int cSquare = to;
    if (capture == Pawn)
    {
      if (type_of(move) == EnPassant)
      {
        cSquare += (Cur == White ? -8 : 8);
        pos->board[cSquare] = PieceNone;
      }
    }
    else
    {
      state->npMaterial[opp] -= PieceValue[capture][PHASE_MG];
    }
    pos->color[opp] ^= BB(cSquare);
    pos->type[capture] ^= BB(cSquare);
#if USE_PIECE_LIST
    auto s = pos->piece[opp][capture][--pos->pCount[opp][capture]];
    UPDATE_PLIST(opp, capture, pos->index[cSquare], s);
    pos->piece[opp][capture][pos->pCount[opp][capture]] = SquareNone;
#endif
    state->score -= SquareValue[capture][opp][cSquare];
    key ^= Zob_psq[opp][capture][cSquare];
    state->rule50 = 0;
  }

  if (state->enPassant != SquareNone)
  {
    key ^= Zob_enpassant[file_of(state->enPassant)];
    state->enPassant = SquareNone;
  }

  // move piece
  pos->color[Cur] ^= bft;
  pos->type[piece] ^= bft;
  pos->board[to] = pos->board[from];
  pos->board[from] = PieceNone;
#if USE_PIECE_LIST
  UPDATE_PLIST(Cur, piece, pos->index[from], to);
#endif
  state->score += SquareValue[piece][Cur][to] - SquareValue[piece][Cur][from];
  key ^= Zob_psq[Cur][piece][from] ^ Zob_psq[Cur][piece][to];

  if (piece == Pawn)
  {
    if ((from ^ to) == 16)
    {
      state->enPassant = to - (Cur == White ? DELTA_N : DELTA_S);
      key ^= Zob_enpassant[file_of(from)];
    }
    if (type_of(move) == Promotion)
    {
      PieceType prom = promotionPiece(move);
      pos->type[Pawn] ^= BB(to);
      pos->type[prom] |= BB(to);
      pos->board[to] = prom;
      state->npMaterial[Cur] += PieceValue[prom][PHASE_MG];
#if USE_PIECE_LIST
      auto s = pos->piece[Cur][Pawn][--pos->pCount[Cur][Pawn]];
      UPDATE_PLIST(Cur, Pawn, pos->index[to], s);
      pos->piece[Cur][Pawn][pos->pCount[Cur][Pawn]] = SquareNone;
      UPDATE_PLIST(Cur, prom, pos->pCount[Cur][prom]++, to);
#endif
      state->score += SquareValue[prom][Cur][to] - SquareValue[Pawn][Cur][to];
      key ^= Zob_psq[Cur][Pawn][to] ^ Zob_psq[Cur][prom][to];
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

  Move move = pos->state->move;
  Square from = from_sq(move);
  Square to = to_sq(move);
  Color opp = pos->to_move;
  Color Cur = ~opp;
  bitboard_t bft = BB(from) ^ BB(to);
  PieceType piece = pos->board[to];
  PieceType capture = pos->state->captured;

  //move_print(move, "undo move ");

  if (type_of(move) == Castle)
  {
    Square s1 = SquareNone, s2 = SquareNone;
    if (to == C1) { s1 = A1; s2 = D1; }
    else if (to == G1) { s1 = H1; s2 = F1; }
    else if (to == C8) { s1 = A8; s2 = D8; }
    else if (to == G8) { s1 = H8; s2 = F8; }
    bitboard_t bft2 = BB(s1) | BB(s2);

    pos->board[s1] = pos->board[s2];
    pos->board[s2] = PieceNone;
    pos->color[Cur] ^= bft2;
    pos->type[Rook] ^= bft2;
    UPDATE_PLIST(Cur, Rook, pos->index[s2], s1);
  }
  else if (type_of(move) == Promotion)
  {
    int prom = promotionPiece(move);
    pos->type[prom] ^= BB(to);
    pos->type[Pawn] |= BB(to);
    pos->board[to] = Pawn;
    piece = Pawn;
    bft = BB(from) ^ BB(to);
#if USE_PIECE_LIST
    auto s = pos->piece[Cur][prom][--pos->pCount[Cur][prom]];
    UPDATE_PLIST(Cur, prom, pos->index[to], s);
    pos->piece[Cur][prom][pos->pCount[Cur][prom]] = SquareNone;
    UPDATE_PLIST(Cur, Pawn, pos->pCount[Cur][Pawn]++, to);
#endif
  }

  // unmove piece
  pos->color[Cur] ^= bft;
  pos->type[piece] ^= bft;
  pos->board[from] = pos->board[to];
  pos->board[to] = PieceNone;
  UPDATE_PLIST(Cur, piece, pos->index[to], from);

  if (capture != PieceNone)
  {
    auto cSquare = to;
    if (type_of(move) == EnPassant)
      cSquare += (Cur == White ? DELTA_S : DELTA_N);
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


Square position_king_square(position_t* pos, Color color)
{
#if USE_PIECE_LIST
  return pos->piece[color][King][0];
#else
  return GLSB(pos->type[King] & pos->color[color]);
#endif
}

/**
 * Get all straight sliding pieces of a color.
 * @param position the position
 * @param color the color of interest
 * @return bitboard with all requested pieces
 */
bitboard_t position_rook_pieces(position_t* pos, Color color)
{
  return pos->color[color] & (pos->type[Rook] | pos->type[Queen]);
}

/**
 * Get all diagonal sliding pieces of a color.
 * @param position the position
 * @param color the color of interest
 * @return bitboard with all requested pieces
 */
bitboard_t position_bishop_pieces(position_t* pos, Color color)
{
  return pos->color[color] & (pos->type[Bishop] | pos->type[Queen]);
}

/**
 * Get all pieces that are pinned against the king.
 * @param position the position
 * @return bitboard of all pinned pieces
 */
bitboard_t position_pinned(position_t* pos)
{
  Color Cur = pos->to_move;
  Color opp = ~Cur;
  bitboard_t myPieces = pos->color[Cur];
  bitboard_t allPieces = pos->color[White] | pos->color[Black];
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
  Color cur = pos->to_move;
  Color opp = ~cur;
  bitboard_t myPieces = pos->color[cur];
  bitboard_t allPieces = pos->color[White] | pos->color[Black];
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
  if (piece == King)
    return B_KingAttacks[square];
  if (piece == Queen)
    return sliding_attack_rook(square, occ) | sliding_attack_bishop(square, occ);
  if (piece == Rook)
    return sliding_attack_rook(square, occ);
  if (piece == Bishop)
    return sliding_attack_bishop(square, occ);
  if (piece == Knight)
    return B_KnightAttacks[square];
  if (piece == Pawn)
  {
    if (pos->color[White] & BB(square))
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
  bitboard_t occ = pos->color[White] | pos->color[Black];
  return position_attackers_to_ext(pos, square, occ);
}

bitboard_t position_attackers_to_ext(position_t* pos, int square, bitboard_t occ)
{
  return (
    (B_KingAttacks[square] & pos->type[King]) |
    (sliding_attack_rook(square, occ) & (pos->type[Rook] | pos->type[Queen])) |
    (sliding_attack_bishop(square, occ) & (pos->type[Bishop] | pos->type[Queen])) |
    (B_KnightAttacks[square] & pos->type[Knight]) |
    (B_WPawnAttacks[square] & pos->color[Black] & pos->type[Pawn]) |
    (B_BPawnAttacks[square] & pos->color[White] & pos->type[Pawn])
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

int position_move_gives_check(position_t* pos, Move move)
{
  Square from = from_sq(move);
  Square to = to_sq(move);
  PieceType piece = type_of(move) == Promotion ? promotionPiece(move) : pos->board[from];
  Color cur = pos->to_move;
  Color opp = ~cur;
  int square = position_king_square(pos, opp);
  bitboard_t occ = (pos->color[White] | pos->color[Black] | BB(to)) ^ BB(from);

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
    if ((piece != Pawn && piece != King) ||
      !squares_aligned(from, to, square))
      return 1;
  }

  if (type_of(move) == NormalMove)
    return 0;

  switch (type_of(move))
  {
  case Promotion:
    return 0;
  case EnPassant:
    // handle the rare case where the two pawns discover a check
    occ ^= BB(make_square(file_of(to), rank_of(from)));
    return (sliding_attack_rook(square, occ) & position_rook_pieces(pos, cur)) |
      (sliding_attack_bishop(square, occ) & position_bishop_pieces(pos, cur));
  case Castle:
  {
    Square s1 = SquareNone, s2 = SquareNone;
    if (to == C1) { s1 = A1; s2 = D1; }
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

  if (pos->type[Pawn] == 0 &&
    state->npMaterial[White] + state->npMaterial[Black] <= PieceValue[Bishop][PHASE_MG])
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

int position_pseudo_is_legal(position_t* pos, Move move)
{
  Color cur = pos->to_move;
  int from = from_sq(move);

  ASSERT_IF_FAIL(pos->state->pinned == position_pinned(pos), "pinned mismatch");

  if (type_of(move) == EnPassant)
  {
    Color opp = ~cur;
    int to = to_sq(move);
    int capSquare = to + (cur == White ? DELTA_S : DELTA_N);
    int kingSquare = position_king_square(pos, cur);
    bitboard_t allPieces = pos->color[White] | pos->color[Black];
    bitboard_t occ = (allPieces ^ (BB(from) | BB(capSquare))) | BB(to);
    return !(sliding_attack_rook(kingSquare, occ) & position_rook_pieces(pos, opp)) &&
      !(sliding_attack_bishop(kingSquare, occ) & position_bishop_pieces(pos, opp));
  }

  if (pos->board[from] == King)
  {
    return type_of(move) == Castle ||
      !(position_attackers_to(pos, to_sq(move)) & pos->color[~cur]);
  }

  return
    !pos->state->pinned ||
    !(pos->state->pinned & BB(from)) ||
    squares_aligned(from, to_sq(move), position_king_square(pos, cur));
}

int position_move_is_pseudo(position_t* pos, Move move)
{
  Color cur = pos->to_move;
  Color opp = ~cur;
  Square from = from_sq(move);
  Square to = to_sq(move);
  PieceType piece = pos->board[from];
  Color fcolor = (pos->color[cur] & BB(from)) ? cur :
    (pos->color[opp] & BB(from)) ? opp : ColorNone;
  Color tcolor = (pos->color[cur] & BB(to)) ? cur :
    (pos->color[opp] & BB(to)) ? opp : ColorNone;
  bitboard_t occ = pos->color[White] | pos->color[Black];

  if (type_of(move) != NormalMove)
    return position_move_is_legal(pos, move);

  //FIXME if (MOVE_PROM(move) - Knight != PieceNone)
  //FIXME   return 0;

  if (piece == PieceNone || fcolor != cur)
    return 0;

  if (tcolor == cur)
    return 0;

  if (piece == Pawn)
  {
    int dir = to - from;
    if ((cur == White) != (dir > 0))
      return 0;

    if (rank_of(to) == Rank8 || rank_of(to) == Rank1)
      return 0;

    switch (dir)
    {
    case DELTA_NW:
    case DELTA_NE:
    case DELTA_SW:
    case DELTA_SE:
      if (tcolor != opp)
        return 0;
      if (abs(file_of(from) - file_of(to)) != 1)
        return 0;
      break;
    case DELTA_N:
    case DELTA_S:
      if (tcolor != ColorNone)
        return 0;
      break;
    case DELTA_NN:
      if (rank_of(to) != Rank4 ||
        tcolor != ColorNone ||
        (BB(from + DELTA_N) & occ))
        return 0;
      break;
    case DELTA_SS:
      if (rank_of(to) != Rank5 ||
        tcolor != ColorNone ||
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
    if (piece != King)
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

int position_move_is_legal(position_t* pos, Move move)
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

int position_see_sign(position_t* pos, Move move)
{
  if (PieceValue[pos->board[to_sq(move)]][PHASE_MG] >=
    PieceValue[pos->board[from_sq(move)]][PHASE_MG])
    return 1;

  return position_see(pos, move);
}

int position_see(position_t* pos, Move move)
{
  int from = from_sq(move);
  int to = to_sq(move);
  bitboard_t occ = (pos->color[White] | pos->color[Black]) ^ BB(from);
  int captured = pos->board[to];

  if (type_of(move) == EnPassant)
  {
    int capSq = to - (pos->to_move == White ? DELTA_N : DELTA_S);
    occ ^= BB(capSq);
    captured = Pawn;
  }
  else if (type_of(move) == Castle)
  {
    return 0;
  }

  if (captured == PieceNone)
    return 0;

  bitboard_t attackers = position_attackers_to_ext(pos, to, occ);

  Color opp = (pos->color[White] & BB(from)) ? Black : White;
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

    swap[index] = -swap[index - 1] + PieceValue[captured][PHASE_MG];
    index++;

    // find least valuable attacker
    int piece;
    for (piece = Pawn; piece <= King; piece++)
    {
      bitboard_t b;
      if ((b = (oppAttackers & pos->type[piece])) != 0)
      {
        occ ^= b & (0 - b);

        if (piece == Pawn || piece == Bishop || piece == Queen)
          attackers |= sliding_attack_bishop(to, occ) & (pos->type[Bishop] | pos->type[Queen]);

        if (piece == Rook || piece == Queen)
          attackers |= sliding_attack_rook(to, occ) & (pos->type[Rook] | pos->type[Queen]);

        captured = piece;
        break;
      }
    }

    attackers &= occ;
    opp = ~opp;
    oppAttackers = attackers & pos->color[opp];

    if (captured == King)
    {
      if (oppAttackers)
        swap[index++] = PieceValue[Queen][PHASE_MG] * 16;
      break;
    }
  } while (oppAttackers);

  while (--index)
  {
    if (-swap[index] < swap[index - 1])
      swap[index - 1] = -swap[index];
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
    if (pos->board[square] != PieceNone)
    {
      Color color = pos->color[White] & BB(square) ? White : Black;
      k ^= Zob_psq[color][pos->board[square]][square];
      //log_line("  piece %16llx  %2d %2d %2d",
      //    Zob_psq[color][pos->board[square]][square],
      //    color, square, pos->board[square]);
    }
  }
  if (state->enPassant != SquareNone)
  {
    k ^= Zob_enpassant[file_of(state->enPassant)];
    //log_line("  enp %llx", Zob_enpassant[file_of(state->enPassant)]);
  }
  if (pos->to_move == Black)
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
    if (piece != PieceNone)
    {
      Color color = pos->color[White] & BB(square) ? White : Black;
      score += SquareValue[piece][color][square];
    }
  }
  return score;
}

int position_is_cap_or_prom(position_t* pos, Move move)
{
  return
    type_of(move) == Promotion ||
    type_of(move) == EnPassant ||
    pos->board[to_sq(move)] != 0;
}

int position_is_cap(position_t* pos, Move move)
{
  return
    type_of(move) == EnPassant ||
    pos->board[to_sq(move)] != 0;
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
  int type, i1;
  for (auto c = White; c <= Black; ++c)
  {
    for (type = Pawn; type <= King; type++)
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
  }

  for (auto c = White; c <= Black; ++c)
  {
    for (type = Pawn; type <= King; type++)
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
  }
#endif
}

int position_pawns_on7(position_t* pos, Color color)
{
  bitboard_t rank7 = color == White ? B_Rank7 : B_Rank2;
  return (pos->type[Pawn] & pos->color[color] & rank7) != 0;
}



