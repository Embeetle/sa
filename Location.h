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

#ifndef __sa_Location_h
#define __sa_Location_h

#include <iostream>

namespace sa {

  // A location in a source file.
  struct Location {

    // Zero-based byte offset from start of file.
    unsigned offset;
    
    Location(): offset(0) {}

    Location(unsigned offset): offset(offset)
    {
    }

    // Location is usable as key for std::set and std::map
    bool operator<(const Location& other) const
    {
      return offset < other.offset;
    }

    // Location is usable as key for std::unordered_set and std::unordered_map
    // A hash function is defined below, near the end of this file.
    bool operator==(const Location& other) const
    {
      return offset == other.offset;
    }

    bool operator!=(const Location& other) const { return !(*this == other); }
    bool operator>(const Location& other) const { return other < *this; }
    bool operator<=(const Location& other) const { return !(*this > other); }
    bool operator>=(const Location& other) const { return !(*this < other); }
  };
  
  inline std::ostream &operator<<(
    std::ostream &out, const sa::Location &location
  )
  {
    return out << "@" << location.offset;
  }
}

template <> struct std::hash<sa::Location> {
  std::size_t operator()(const sa::Location &location) const
  {
    return hash<unsigned>()(location.offset);
  }
};

#endif
