#include "thread.h"
#include "position.h"
#include "interface.h"
#include "search.h"

void IO_enter() {}
void IO_leave() {}

#include <thread>
#include <assert.h>

namespace walce
{
  SearchThread Searcher;

  Thread::Thread()
    : _thread(&Thread::mainLoop, this)
    , _mutex()
    , _condition()
  {
  }

  Thread::~Thread()
  {
    assert(!_running);
    // wake up to leave idle loop
    _exit = true;
    wakeUp();
    _thread.join();
  }

  void Thread::mainLoop()
  {
    while (true)
    {
      std::unique_lock<std::mutex> lock(_mutex);
      _running = false;
      _condition.notify_one();
      _condition.wait(lock, [&] { return _running; });

      if (_exit)
        return;

      lock.unlock();

      action();
    }
  }

  void Thread::wakeUp()
  {
    std::unique_lock<std::mutex> lock(_mutex);
    _running = true;
    _condition.notify_one();
  }

  void Thread::wait()
  {
    std::unique_lock<std::mutex> lock(_mutex);
    _condition.wait(lock, [&] { return !_running; });
  }


  SearchThread::SearchThread()
    : Thread()
    , _pos(nullptr)
  {
  }
  
  void SearchThread::searchPosition(position_t* pos)
  {
    assert(!_running);
    _pos = pos;
    wakeUp();
  }

  void SearchThread::action()
  {
    Move move = think(_pos);
    IF.search_done(_pos, move); //FIXME: process outside thread
  }

}
