#ifndef THREAD_H__
#define THREAD_H__

#include <pthread.h>
#include "position.h"

void threads_init(position_t* pos);
void threads_exit();

void threads_search(void);
void threads_search_stop(void);

void IO_enter();
void IO_leave();

#endif