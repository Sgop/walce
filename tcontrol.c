#include <string.h>
#include "tcontrol.h"
#include "stats.h"

tcontrol_t TC;

void timer_start(ctimer_t* timer)
{
    gettimeofday(&timer->start, NULL);
}

int timer_get(ctimer_t* timer)
{
    unsigned ms = 0;
    unsigned s = 0;
    struct timeval now;

    gettimeofday(&now, NULL);
    s = now.tv_sec - timer->start.tv_sec;
    ms = (now.tv_usec - timer->start.tv_usec) / 1000;
    return s * 1000 + ms;
}


void TC_clear(void)
{
    memset(&TC, 0, sizeof(TC));
}

void TC_start(void)
{
    TC.stop = 0;
    timer_start(&TC.timer);
}

int TC_get_time(void)
{
    return timer_get(&TC.timer);
}

int TC_have_more_time()
{
    int elapsed = TC_get_time();
    
    if (TC.infinite)
        return 1;
        
//    if (TC.l_nodes > 0)
//        return stats_get(ST_NODE) >= TC.l_nodes;

    if (TC.l_time > 0)
        return elapsed < TC.l_time;

    if (TC.ctime[0] <= 1)
        return elapsed < 1000;
    else if (TC.ctime[0] <= 10000)
        return elapsed < 100;
    else
        return (elapsed - TC.ctime[1] / 5) * 45 < TC.ctime[0];
}


