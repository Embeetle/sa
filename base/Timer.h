// Copyright © 2018-2026 Johan Cockx
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// SPDX-License-Identifier: GPL-3.0-or-later

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
