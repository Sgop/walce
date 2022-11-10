#ifndef TCONTROL_H__
#define TCONTROL_H__

#ifdef _WIN32
#include <Windows.h>
#include <stdint.h> // portable: uint64_t   MSVC: __int64 
#include <winsock.h>
int gettimeofday(struct timeval* tp, struct timezone* tzp);
#else
#include <sys/time.h>
#endif


typedef struct {
  struct timeval start;
} ctimer_t;

typedef struct {
  int ctime[2];
  int otime[2];
  int togo;
  int l_depth;
  int l_nodes;
  int l_time;
  int infinite;

  int stop;
  ctimer_t timer;
} tcontrol_t;

extern tcontrol_t TC;

void timer_start(ctimer_t* timer);
int timer_get(ctimer_t* timer);

void TC_clear(void);
void TC_start(void);
int TC_get_time(void);
int TC_have_more_time(void);

#endif

