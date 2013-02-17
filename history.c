#include <string.h>
#include <stdlib.h>
#include "history.h"


void history_clear(history_t* hist)
{
    memset(hist->data, 0, C_NUM * P_NUM * SQ_NUM *sizeof(int));
    memset(hist->gain, 0, C_NUM * P_NUM * SQ_NUM *sizeof(int));
}

int history_value(history_t* hist, color_t c, int p, int s)
{
    return hist->data[c][p][s];
}

int history_gain(history_t* hist, color_t c, int p, int s)
{
    return hist->gain[c][p][s];
}

void history_add(history_t* hist, color_t c, int p, int s, int add)
{
    if (abs(hist->data[c][p][s] + add) < HIST_MAX_VAL)
        hist->data[c][p][s] += add;
}

void history_update_gain(history_t* hist, color_t c, int p, int s, int gain)
{
    hist->gain[c][p][s]--;
    if (gain > hist->gain[c][p][s])
        hist->gain[c][p][s] = gain;
}


