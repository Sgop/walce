#include "tcontrol.h"

namespace walce {

  TimeControl TC;

  TimeControl::TimeControl()
    : _playerTimes()
    , _moveTime()
    , _moveDepth(0)
    , _moveNodes(0)
    , _infinite(false)
    , _start()
    , _started(false)
  {}

  void TimeControl::setTime(Color color, std::chrono::milliseconds t)
  {
    _playerTimes[color][0] = t;
  }

  void TimeControl::setIncTime(Color color, std::chrono::milliseconds t)
  {
    _playerTimes[color][1] = t;
  }

  void TimeControl::reset()
  {
    _moveTime = std::chrono::milliseconds(0);
    _moveDepth = 0;
    _moveNodes = 0;
    _infinite = false;
  }

  void TimeControl::start()
  {
    std::chrono::high_resolution_clock t;
    _start = t.now();
    _started = true;
  }

  void TimeControl::stop()
  {
    _started = false;
  }

  std::chrono::milliseconds TimeControl::elapsed() const
  {
    std::chrono::high_resolution_clock t;
    return std::chrono::duration_cast<std::chrono::milliseconds>(t.now() - _start);
  }

  bool TimeControl::active() const
  {
    return _started;
  }

  int TimeControl::getMaxPly() const
  {
    return _moveDepth == 0 || _moveDepth > MAX_PLY ? MAX_PLY : _moveDepth;

  }

  bool TimeControl::haveMoreTime() const
  {
    if (_infinite)
      return true;

    auto e = elapsed();

    //    if (TC.l_nodes > 0)
    //        return stats_get(ST_NODE) >= TC.l_nodes;

    if (_moveTime.count() > 0)
      return e < _moveTime;

    if (_playerTimes[White][0].count() <= 1)
      return e.count() < 1000;
    else if (TC._playerTimes[White][0].count() <= 10000)
      return e.count() < 100;
    else
      return (e - TC._playerTimes[White][1] / 5) * 10 < TC._playerTimes[White][0];
  }

  void TimeControl::setMoveTime(std::chrono::milliseconds t)
  {
    _moveTime = t;
  }

  void TimeControl::setMoveDepth(unsigned d)
  {
    _moveDepth = d;
  }

  void TimeControl::setMoveNodes(unsigned nodes)
  {
    _moveNodes = nodes;
  }
  
  void TimeControl::setInfinite(bool val)
  {
    _infinite = val;
  }

  std::string TimeControl::printThink() const
  {
    char str[1024];

    sprintf(str, "Thinking %s(depth: %d)(time: %lld ms)(node: %d)(time: %.1f/%.1f s)",
      _infinite ? "infinite " : "", _moveDepth, _moveTime.count(), _moveNodes,
      _playerTimes[White][0].count() / 1000.0,
      _playerTimes[White][1].count() / 1000.0);
    return str;
  }


}

