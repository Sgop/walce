#ifndef HISTORY_H__
#define HISTORY_H__

#include "types.h"

typedef struct {
    int data[C_NUM][P_NUM][SQ_NUM];
    int gain[C_NUM][P_NUM][SQ_NUM];
} history_t;

#define HIST_MAX_VAL 2000

void history_clear(history_t* hist);

int history_value(history_t* hist, color_t c, int p, int s);
int history_gain(history_t* hist, color_t c, int p, int s);

void history_update_gain(history_t* hist, color_t c, int p, int s, int gain);
void history_add(history_t* hist, color_t c, int p, int s, int add);

#endif

