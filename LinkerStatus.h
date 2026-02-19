// Copyright 2018-2024 Johan Cockx
#ifndef __LinkerStatus_h
#define __LinkerStatus_h

#include <iostream>

namespace sa {

  // Linker status enum
  typedef enum {
    // Link analysis is scheduled; file inclusion status is not final yet.
    LinkerStatus_waiting,
    
    // Link analysis is in progress; file inclusion status is not final yet.
    LinkerStatus_busy,
    
    // Link analysis is done; file inclusion status has been determined and no
    // undefined or multiply defined globals have been found.
    LinkerStatus_done,
    
    // Link analysis is done; file inclusion status has been determined and
    // undefined or multiply defined globals have been found.
    LinkerStatus_error,
  } LinkerStatus;
  
  static const char * const LinkerStatus_names[] = {
    "waiting", "busy", "done", "error"
  };

  inline std::ostream &operator<<(std::ostream &out, LinkerStatus status)
  {
    return out << LinkerStatus_names[status];
  }
}

#endif
