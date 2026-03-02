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

#ifndef __sa_FileLocation_h
#define __sa_FileLocation_h

#include "base/ptr.h"
#include "File.h"

namespace sa {
  
  struct FileLocation: public Location {

    base::ptr<File> file;

    FileLocation() {}
    
    FileLocation(base::ptr<File> file, unsigned offset)
      : Location(offset), file(file)
    {
    }

    FileLocation(
      base::ptr<File> file,
      const Location &location
    ): Location(location), file(file)
    {
    }

    // FileLocation is usable as key for std::set and std::map
    bool operator<(const FileLocation& other) const
    {
      return file != other.file ? file < other.file :
        (const Location&)*this < (const Location&)other;
    }
      
    // FileLocation is usable as key for std::unordered_set and
    // std::unordered_map A hash function is defined below, near the end of this
    // file.
    bool operator==(const FileLocation& other) const
    {
      return file == other.file
        && (const Location&)*this == (const Location&)other;
    }

    bool operator!=(const FileLocation& other) const { return !(*this == other);}
    bool operator>(const FileLocation& other) const { return other < *this; }
    bool operator<=(const FileLocation& other) const { return !(*this > other); }
    bool operator>=(const FileLocation& other) const { return !(*this < other); }
  };

  // Implementation in File.cpp
  std::ostream &operator<<(std::ostream &out, const sa::FileLocation &location);
}

template <> struct std::hash<sa::FileLocation> {
  std::size_t operator()(const sa::FileLocation &location) const
  {
    return std::hash<sa::File*>()((sa::File*)(location.file))
      ^ hash<sa::Location>()(location);
  }
};

#endif
