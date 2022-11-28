#include <stdio.h>
#include <string>
#include "stats.h"
#include "log.h"
#include "tcontrol.h"

using namespace walce;

static unsigned Stats[ST_NUM];
static std::string StatsStr[ST_NUM] = {
  "nodes visited",
  "generated move",
  "static evaluation",
  "calc pinned",
};

void stats_start(void)
{
    memset(Stats, 0, ST_NUM * sizeof(*Stats));
    TC.start();
}

void stats_print(void)
{
    auto ms = TC.elapsed().count() + 1;
    
    log_line("--STATS-------------------------------------------");
    log_line(" %u,%03u seconds", ms/1000, ms%1000);
    for (int i1 = 0; i1 < ST_NUM; i1++)
    {
        log_line(" %u %s (%u per ms)", Stats[i1], StatsStr[i1].c_str(), Stats[i1] / ms);
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
