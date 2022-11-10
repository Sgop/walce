#include <string.h>
#include "tcontrol.h"
#include "stats.h"

#ifdef _WIN32

int gettimeofday(struct timeval* tp, struct timezone* tzp)
{
  // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
  // This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
  // until 00:00:00 January 1, 1970 
  static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);

  SYSTEMTIME  system_time;
  FILETIME    file_time;
  uint64_t    time;

  GetSystemTime(&system_time);
  SystemTimeToFileTime(&system_time, &file_time);
  time = ((uint64_t)file_time.dwLowDateTime);
  time += ((uint64_t)file_time.dwHighDateTime) << 32;

  tp->tv_sec = (long)((time - EPOCH) / 10000000L);
  tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
  return 0;
}


#endif

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


