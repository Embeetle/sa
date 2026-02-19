// Copyright 2018-2024 Johan Cockx
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
