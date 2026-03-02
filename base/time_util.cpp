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

#include "time_util.h"
#include "debug.h"
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <iomanip>
#include <set>

double base::get_time()
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  static struct timespec start;
  if (start.tv_sec == 0) {
    start = now;
  }
  time_t sec = now.tv_sec - start.tv_sec;
  long nsec = now.tv_nsec - start.tv_nsec;
  return sec + nsec * 1e-9;
}

void base::sleep(double time)
{
  usleep(time*1e6);
}

void base::sleep_until(double time)
{
  for (;;) {
    double now = get_time();
    if (now >= time) break;
    sleep(time - now);
  }
}

void base::print_iso_timestamp_to_buffer(char *buffer)
{
  time_t now;
  struct tm* tm_info;
  time(&now);
  tm_info = localtime(&now);
  strftime(buffer, iso_timestamp_size, "%Y-%m-%d_%H:%M:%S", tm_info);
}
