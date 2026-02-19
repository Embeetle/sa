// Copyright 2018-2023 Johan Cockx
#ifndef __base_Timer_h
#define __base_Timer_h

#include <mutex>
#include <iostream>
#include "time_util.h"
#include "debug.h"

namespace base {
  class Timer: public NoCopy, public base::Checked {
    const char *label;
    bool average;
    double time = 0;
    unsigned count = 0;
    unsigned running = false;
    mutable std::mutex _mutex;
  protected:
    void _report(double now);
    void _reset(double now);
    double _current(double now) const;
    friend std::ostream &operator<<(std::ostream &out, const Timer &timer);
  public:
    // Average: print average info for this timer
    Timer(const char *label, bool average = true);
    ~Timer();
    void start();
    void stop();
    double current() const;
    void report();
    static void report_all();

    class Scope: public base::NoCopy, public base::Checked {
      Timer &timer;
    public:
      Scope(Timer &timer): timer(timer)
      {
        timer.start();
      }
      ~Scope()
      {
        timer.stop();
      }
    };
  };
  
}

#endif
