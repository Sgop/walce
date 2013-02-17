#ifndef STATS_H__
#define STATS_H__

enum {
    ST_NODE,
    ST_MOVE_GEN,
    ST_EVAL,
    ST_PINNED,
    ST_NUM,
};

void stats_start(void);
void stats_print(void);
void stats_inc(int mode, int num);
int stats_get(int mode);

#endif
