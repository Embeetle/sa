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

#ifndef __time_util_h
#define __time_util_h

namespace base {
  
  // Get the elapsed time in seconds since program startup.
  double get_time();

  // Sleep the specified amount of time in seconds, allowing other threads and
  // processes to run. In contrast to the standard sleep function, time can be
  // specified as a floating point number.
  void sleep(double time);

  // Sleep until get_time() returns the specified time or later. Return
  // immediately if the specified time is in the past.
  void sleep_until(double time);

  //  Print a timestamp in ISO format (YYYY-mm-dd HH:MM:SS) to the given buffer.
  enum { iso_timestamp_size = 20 };
  void print_iso_timestamp_to_buffer(char *buffer);
}

#endif
