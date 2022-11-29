#ifndef THREAD_H__
#define THREAD_H__

#include "position.h"
#include <thread>
#include <mutex>
#include <condition_variable>

void IO_enter();
void IO_leave();


namespace walce
{

  class Thread
  {
  public:
    Thread();
    virtual ~Thread();

    virtual void action() = 0;

    virtual void mainLoop() final;
    virtual void wakeUp() final;
    virtual void wait() final;

    std::atomic_bool stop;

  protected:
    std::thread _thread;
    std::mutex _mutex;
    std::condition_variable _condition;
    bool _running = true;
    bool _exit = false;
  };

  class SearchThread : public Thread
  {
  public:
    SearchThread();

    void searchPosition(position_t* pos);
    virtual void action() override;

  protected:
    std::atomic<position_t*> _pos;
  };

  extern SearchThread Searcher;
}


#endif