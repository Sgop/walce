#include "thread.h"
#include "types.h"
#include "interface.h"
#include "search.h"
#include "tcontrol.h"

typedef struct {
    pthread_t handle;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    int searching;
    int exit;
} search_thread_t;

static search_thread_t SearchThread;
static position_t* Position;
static pthread_mutex_t IOMutex;

static void* search_main(void* arg)
{
    search_thread_t* thread = arg;
    move_t move;
    
    thread->exit = 0;
    thread->searching = 0;
    
    while (1)
    {
        pthread_cond_wait(&thread->cond, &thread->mutex);
        if (thread->exit)
            break;
        thread->searching = 1;
        move = think(Position);
        IF.search_done(Position, move);
        thread->searching = 0;
    }
    return NULL;
}

static void search_wakeup()
{
    pthread_cond_signal(&SearchThread.cond);
}

void threads_init(position_t* pos)
{
    Position = pos;
    pthread_mutex_init(&SearchThread.mutex, NULL);
    pthread_cond_init(&SearchThread.cond, NULL);
    pthread_create(&SearchThread.handle, NULL, search_main, &SearchThread);
    pthread_mutex_init(&IOMutex, NULL);
}

void threads_exit()
{
    SearchThread.exit = 1;
    search_wakeup();
    pthread_join(SearchThread.handle, NULL);
}

void threads_search()
{
    TC.stop = 0;
    search_wakeup();
}

void threads_search_stop()
{
    TC.stop = 1;
    pthread_mutex_lock(&SearchThread.mutex);
    pthread_mutex_unlock(&SearchThread.mutex);
}

void IO_enter()
{
    pthread_mutex_lock(&IOMutex);
}

void IO_leave()
{
    pthread_mutex_unlock(&IOMutex);
}


