#ifndef HISTORY_H__
#define HISTORY_H__

#include "types.h"

typedef struct {
    int data[ColorNum][PieceNum][SquareNum];
    int gain[ColorNum][PieceNum][SquareNum];
} history_t;

#define HIST_MAX_VAL 2000

void history_clear(history_t* hist);

int history_value(history_t* hist, Color c, int p, int s);
int history_gain(history_t* hist, Color c, int p, int s);

void history_update_gain(history_t* hist, Color c, int p, int s, int gain);
void history_add(history_t* hist, Color c, int p, int s, int add);

#endif

