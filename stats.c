#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "stats.h"
#include "log.h"
#include "tcontrol.h"

static unsigned Stats[ST_NUM];
static char* StatsStr[ST_NUM] = {
  "nodes visited",
  "generated move",
  "static evaluation",
  "calc pinned",
};

void stats_start(void)
{
    memset(Stats, 0, ST_NUM * sizeof(*Stats));
    TC_start();
}

void stats_print(void)
{
    unsigned ms = TC_get_time() + 1;
    
    log_line("--STATS-------------------------------------------");
    log_line(" %u,%03u seconds", ms/1000, ms%1000);
    int i1;
    for (i1 = 0; i1 < ST_NUM; i1++)
    {
        log_line(" %u %s (%u per ms)", Stats[i1], StatsStr[i1], Stats[i1] / ms);
    }
    log_line("--------------------------------------------------");
}

void stats_inc(int mode, int num)
{
    Stats[mode] += num;
}

int stats_get(int mode)
{
    return Stats[mode];
}
