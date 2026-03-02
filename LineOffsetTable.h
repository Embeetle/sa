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

#ifndef __sa_LineOffsetTable_h
#define __sa_LineOffsetTable_h

#include <vector>
#include <iostream>

namespace sa {

  // A table to convert (line,column) positions in a file to byte offsets in
  // constant time. Construction takes time proportional to the size of the
  // file.
  //
  // A file with n end-of-line characters (\n) has n+1 lines.  The first line
  // has offset zero.  All lines except for the last line include an end-of-line
  // character; this is reflected in the line length.  If the file ends with an
  // end-of-line character, the last line is empty, i.e. has zero length.
  //
  class LineOffsetTable {

    // Byte offset of the first character of a line, indexed by line number.
    // The first entry is always zero. The last entry is a sentinel containing
    // the number of bytes in the file and does not represent a line.  If the
    // file ends with an end-of-line character, the one-before-last and last
    // entries are equal.
    std::vector<unsigned> offsets;

  public:
    // Create a file offset table for file contents read from an istream.
    LineOffsetTable(std::istream &in);

    // Return the number of lines in the file.  This is one more than the number
    // of end-of-line characters (\n).  In other words, a file without
    // end-of-line characters consists of one line, with line number zero. If
    // the file contents end with an end-of-line character, then the last line
    // is empty.
    unsigned line_count() const;

    // Return the length of a line. The line number must be less than the line
    // count. Line length includes the trailing end-of-line character if
    // present.
    unsigned line_length(unsigned line) const;

    // Return true iff the given (line,column) position corresponds to a
    // position in the file. Line and column are both zero-based.
    bool in_file(unsigned line, unsigned column) const;

    // Return the byte offset from the start of the file for the given
    // (line,column) position. The first character has offset zero.
    unsigned offset(unsigned line, unsigned column) const;

    // Return the byte offset from the start of the file for the given
    // (line,column) position, or zero if that position is not in the file.
    unsigned offset_or_zero(unsigned line, unsigned column) const;
  };
}

#endif
