#ifndef TCONTROL_H__
#define TCONTROL_H__

#include "types.h"
#include <chrono>
#include <string>

namespace walce {

  class TimeControl {
  public:
    typedef std::chrono::duration<unsigned, std::chrono::milliseconds> Duration;
    typedef std::chrono::time_point<unsigned, std::chrono::milliseconds> Time;
    typedef std::chrono::high_resolution_clock Timer;

    TimeControl();

    void setTime(Color color, std::chrono::milliseconds t);
    void setIncTime(Color color, std::chrono::milliseconds t);
    void setMoveTime(std::chrono::milliseconds t);
    void setMoveDepth(unsigned d);
    void setMoveNodes(unsigned nodes);
    void setInfinite(bool val);

    void reset();
    void start();
    void stop();

    std::chrono::milliseconds elapsed() const;
    bool active() const;
    int getMaxPly() const;
    bool haveMoreTime() const;

    std::string printThink() const;

  protected:
    std::chrono::milliseconds _playerTimes[ColorNum][2];
    std::chrono::milliseconds _moveTime;
    unsigned _moveDepth;
    unsigned _moveNodes;
    bool _infinite;
    std::chrono::steady_clock::time_point _start;
    bool _started;
  };

  extern TimeControl TC;

}

#endif

