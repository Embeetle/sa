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
