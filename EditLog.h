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

#ifndef __sa_EditLog_h
#define __sa_EditLog_h

#include "Range.h"
#include <vector>
#include <iostream>

namespace sa {

  // A log of edits applied to a file, supporting mapping of pre-edit offsets to
  // post-edit offsets and back.
  //
  class EditLog {

    struct Entry {
      unsigned offset;
      unsigned remove;
      unsigned insert;
      Entry(unsigned offset, unsigned remove, unsigned insert):
        offset(offset), remove(remove), insert(insert)
      {}
    };
    friend std::ostream &operator<<(
      std::ostream &out, const sa::EditLog::Entry &entry
    );

    std::vector<Entry> log;

  public:

    EditLog();

    void edit(Range range, const char *new_text);

    void clear();

    // Apply edits to pre-edit range.  Resulting post-edit range is void iff
    // any part of the pre-edit range is edited.
    Range apply(Range range) const;

    // Revert edits on post-edit range. Resulting pre-edit range is void iff
    // any part of the post-edit range is edited.
    Range revert(Range range) const;

    // Map from pre-edit offset to post-edit offset
    unsigned apply(unsigned offset) const;

    // Map from post-edit offset to pre-edit offset
    unsigned revert(unsigned offset) const;

  protected:
    void insert(unsigned offset, unsigned remove, unsigned insert);

  };

  std::ostream &operator<<(
    std::ostream &out, const sa::EditLog::Entry &entry
  );
}

#endif
