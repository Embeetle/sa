// Copyright 2018-2023 Johan Cockx
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
