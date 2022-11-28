#include "thread.h"
#include "types.h"
#include "interface.h"
#include "search.h"
#include "tcontrol.h"

using namespace walce;

#ifdef _WIN32
typedef HANDLE mutex_t;
#else
typedef pthread_mutex_t mutex_t;
#endif

#ifdef _WIN32
typedef CONDITION_VARIABLE cond_t;
#else
typedef pthread_cond_t cond_t;
#endif

#ifdef _WIN32
typedef HANDLE handle_t;
#else
typedef pthread_t handle_t;
#endif

typedef struct {
  handle_t handle;
  cond_t cond;
#ifdef _WIN32
  CRITICAL_SECTION critCond;
#endif
  mutex_t mutex;
  int searching;
  int exit;
} thread_t;

static thread_t SearchThread;
static position_t* Position;
static mutex_t IOMutex;

static
#ifdef  _WIN32
DWORD WINAPI
#else
void*
#endif
search_main(void* arg)
{
  thread_t* thread = (thread_t*)arg;
  Move move;

  thread->exit = 0;
  thread->searching = 0;

  while (1)
  {
#ifdef _WIN32
    SleepConditionVariableCS(&thread->cond, &thread->critCond, INFINITE);
#else
    pthread_cond_wait(&thread->cond, &thread->mutex);
#endif
    if (thread->exit)
      break;
    thread->searching = 1;
    move = think(Position);
    IF.search_done(Position, move);
    thread->searching = 0;
  }
#ifdef  _WIN32
  return 0;
#else
  return NULL;
#endif
}

static void search_wakeup()
{
#ifdef _WIN32
  WakeConditionVariable(&SearchThread.cond);
#else
  pthread_cond_signal(&SearchThread.cond);
#endif
}

void threads_init(position_t* pos)
{
  Position = pos;
#ifdef _WIN32
  SearchThread.mutex = CreateMutex(NULL, FALSE, NULL);
  InitializeConditionVariable(&SearchThread.cond);
  InitializeCriticalSection(&SearchThread.critCond);
  DWORD uiThredId;
  SearchThread.handle = CreateThread(0, 0, search_main, &SearchThread, 0, &uiThredId);
  IOMutex = CreateMutex(NULL, FALSE, NULL);
#else
  pthread_mutex_init(&SearchThread.mutex, NULL);
  pthread_cond_init(&SearchThread.cond, NULL);
  pthread_create(&SearchThread.handle, NULL, search_main, &SearchThread);
  pthread_mutex_init(&IOMutex, NULL);
#endif
}

void threads_exit()
{
  SearchThread.exit = 1;
  search_wakeup();
  //pthread_join(SearchThread.handle, NULL);
}

void threads_search()
{
  TC.start();
  search_wakeup();
}

void threads_search_stop()
{
  TC.stop();
#ifdef _WIN32
  WaitForSingleObject(SearchThread.mutex, INFINITE);
  ReleaseMutex(SearchThread.mutex);
#else
  pthread_mutex_lock(&SearchThread.mutex);
  pthread_mutex_unlock(&SearchThread.mutex);
#endif
}

void IO_enter()
{
#ifdef _WIN32
  WaitForSingleObject(IOMutex, INFINITE);
#else
  pthread_mutex_lock(&IOMutex);
#endif
}

void IO_leave()
{
#ifdef _WIN32
  ReleaseMutex(IOMutex);
#else
  pthread_mutex_unlock(&IOMutex);
#endif
}
