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

#ifndef __OccurrenceStyle_h
#define __OccurrenceStyle_h

namespace sa {
  // Symbol access kind enum - specifies how an occurrence accesses a symbol.
  enum OccurrenceStyle {
    OccurrenceStyle_unspecified,        // u 0
    OccurrenceStyle_data,               // d 1
    OccurrenceStyle_function,           // f 2
    OccurrenceStyle_virtual_function,   // v 3 
  };
  
  static const char * const OccurrenceStyle_names[] = {
    "unspecified", "data", "function", "virtual function",
  };

  inline std::ostream &operator<<(std::ostream &out, sa::OccurrenceStyle kind)
  {
    return out << sa::OccurrenceStyle_names[kind];
  }
}

#endif
