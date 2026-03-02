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

#include "Timer.h"
#include "time_util.h"
#include <iostream>
#include <iomanip>
#include <set>

static std::mutex mutex;

#include "debug.h"

#define USE_TIMERS

static std::set<base::Timer*> *all_timers = 0;

base::Timer::Timer(const char *label, bool average)
  : label(label), average(average)
{
#ifdef USE_TIMERS
  std::lock_guard<std::mutex> lock(mutex);
  trace_nest("Timer::Timer " << label << " " << get_tid());
  if (!all_timers) all_timers = new std::set<base::Timer*>;
  all_timers->insert(this);
#endif
}

base::Timer::~Timer()
{
#ifdef USE_TIMERS
  std::lock_guard<std::mutex> lock(mutex);
  trace_nest("Timer::~Timer " << label << " " << get_tid());
  {
    std::lock_guard<std::mutex> _lock(_mutex);
    _report(get_time());
  }
  all_timers->erase(this);
#endif
}

namespace base {
  std::ostream &operator<<(std::ostream &out, const Timer &timer)
  {
    return out << timer.label << " " << timer._current(get_time()) << " "
               << timer.count << " " << timer.running;
  }
}

double base::Timer::current() const
{
  std::lock_guard<std::mutex> _lock(_mutex);
  return _current(get_time());
}

double base::Timer::_current(double now) const
{
  return time + running * now;
}

void base::Timer::start()
{
#ifdef USE_TIMERS
  std::lock_guard<std::mutex> _lock(_mutex);
  double now = get_time();
  ++running;
  time -= now;
  count++;
  trace("Timer::start " << *this);
#endif
}

void base::Timer::stop()
{
#ifdef USE_TIMERS
  std::lock_guard<std::mutex> _lock(_mutex);
  trace("Timer::stop " << *this);
  double now = get_time();
  time += now;
  assert(running);
  --running;
#endif
}

void base::Timer::report()
{
  std::lock_guard<std::mutex> lock(mutex);
  std::lock_guard<std::mutex> _lock(_mutex);
  _report(get_time());
}

void base::Timer::_report(double now)
{
#ifdef USE_TIMERS
  // Do not use atomic output here
  debug_exec(
  trace_nest("Timer::report " << *this);
  if (count) {
    std::cout
      << std::setw(25) << label << ": "
      << std::fixed << std::setw(5) << std::setprecision(1) << _current(now)
      << "s";
    if (average && !running) {
      std::cout
        << " total, "
        << std::setw(5) << std::setprecision(1) << (1e3*_current(now)/count)
        << "ms average over " << count;
    }
    std::cout << std::defaultfloat << std::endl;
    trace_code(std::flush(std::cout));
  }
  );
#else
  (void)now;
#endif
}

void base::Timer::_reset(double now)
{
#ifdef USE_TIMERS
  trace("Timer::reset " << *this);
  time = running * -now;
  count = 0;
  trace("   reset to " << *this);
#else
  (void)now;
#endif
}

void base::Timer::report_all()
{
  std::lock_guard<std::mutex> lock(mutex);
  if (all_timers) {
    double now = get_time();
    for (auto timer: *all_timers) {
      std::lock_guard<std::mutex> _lock(timer->_mutex);
      timer->_report(now);
      timer->_reset(now);
    }
  }
}
