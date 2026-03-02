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

#include "EditLog.h"
#include "base/debug.h"
#include <string.h>

namespace sa {
}

std::ostream &sa::operator<<(std::ostream &out, const sa::EditLog::Entry &entry)
{
  out <<"(" << entry.offset << "-" << entry.remove << "+" << entry.insert <<")";
  return out;
}

sa::EditLog::EditLog()
{
}

static inline unsigned sub_floor(unsigned a, unsigned b)
{
  return b < a ? a - b : 0;
}

void sa::EditLog::insert(unsigned offset, unsigned remove, unsigned insert)
{
  if (!log.empty()) {
    Entry &entry = log.back();
    trace("try merge " << offset << " " << remove << " " << insert
      << " with " << entry.offset << " " << entry.remove << " " << entry.insert
    );
    if (offset + remove >= entry.offset
      && offset <= entry.offset + entry.insert
    ) {
      // Merge is possible, because the range of chars to be removed
      // offset..offset+remove touches or overlaps with the range of chars
      // inserted by the previous entry entry.offset..entry.offset+entry.insert
      //
      // #chars to remove before chars inserted by the previous entry
      unsigned pre_remove = sub_floor(entry.offset, offset);
      //
      // #chars to remove after chars inserted by the previous entry
      unsigned post_remove = sub_floor(
        offset + remove, entry.offset + entry.insert
      );
      entry.insert += insert + (pre_remove + post_remove) - remove;
      entry.remove += (pre_remove + post_remove);
      entry.offset -= pre_remove;
      trace("EditLog: mod " << log.back());
      return;
    }
  }
  log.emplace_back(offset, remove, insert);
  trace("EditLog: add " << log.back());
}

void sa::EditLog::edit(Range range, const char *new_text)
{
  trace("EditLog: replace " << range << " by " << new_text);
  assert(new_text);
  insert(range.begin, range.size(), strlen(new_text));
}

void sa::EditLog::clear()
{
  log.clear();
}

unsigned sa::EditLog::apply(unsigned offset) const
{
  trace("EditLog apply " << offset);
  for (auto const &entry: log) {
    trace("  " << offset << " apply " << entry);
    if (offset >= entry.offset) {
      if (offset < entry.offset + entry.remove) {
        // Original location no longer exists - take edit location instead
        offset = entry.offset;
      } else {
        // Original location has shifted due to edit before it
        offset = offset - entry.remove + entry.insert;
      }
    }
  }
  trace("> " << offset);
  return offset;
}

sa::Range sa::EditLog::apply(Range range) const
{
  trace("EditLog apply " << range);
  if (!range.is_void()) {
    for (auto const &entry: log) {
      trace("  " << range << " apply " << entry);
      if (entry.offset < range.end) {
        // There is an effect
        if (range.begin < entry.offset + entry.remove) {
          // Pre-edit range is edited - return void range
          range.set_void();
        } else {
          // Apply shift to range due to edit before it
          int shift = entry.insert - entry.remove;
          range.begin += shift;
          range.end += shift;
        }
      }
    }
  }
  trace("> " << range);
  return range;
}

unsigned sa::EditLog::revert(unsigned offset) const
{
  trace("EditLog revert " << offset);
  for (size_t i = log.size(); i--; ) {
    Entry const &entry = log[i];
    trace("   " << offset << " revert " << entry);
    if (entry.offset <= offset) {
      if (offset < entry.offset + entry.insert) {
        // Location did not exist originally - take edit location instead
        offset = entry.offset;
      } else {
        // Undo location shift due to edit before it
        offset = offset - entry.insert + entry.remove;
      }
    }
  }
  trace("> " << offset);
  return offset;
}

sa::Range sa::EditLog::revert(Range range) const
{
  trace("EditLog revert " << range);
  if (!range.is_void()) {
    for (size_t i = log.size(); i--; ) {
      Entry const &entry = log[i];
      trace("   " << range << " revert " << entry);
      if (entry.offset < range.end) {
        // There is an effect
        if (range.begin < entry.offset + entry.insert) {
          // Post-edit range was edited  - return void range
          range.set_void();
        } else {
          // Undo shift of range due to edit before it
          int shift = entry.insert - entry.remove;
          range.begin -= shift;
          range.end -= shift;
        }
      }
    }
  }
  trace("> " << range);
  return range;
}

#ifdef SELFTEST

int main()
{
  std::cout << "Hello\n";
  sa::EditLog log;
  for (unsigned i = 0; i < 8; ++i) {
    assert_(log.apply(i) == i, i);
    assert_(log.revert(i) == i, i);
  }
  log.edit(4,6,"hello");
  for (unsigned i = 0; i < 4; ++i) {
    assert_(log.apply(i) == i, i);
  }
  unsigned a4 = log.apply(4);
  unsigned a5 = log.apply(5);
  test_out("4 --> " << a4);
  test_out("5 --> " << a5);
  for (unsigned i = 6; i < 12; ++i) {
    unsigned post = log.apply(i);
    assert_(post == i+3, i << " --> " << post);
  }
  for (unsigned i = 0; i < 12; ++i) {
    test_out("");
    test_out("test " << i);
    unsigned j = log.apply(i);
    unsigned pre = log.revert(j);
    unsigned post = log.apply(pre);
    assert_(post == j, i << " -> " << j << " <- " << pre << " -> " << post);
  }
  return 0;
}

#endif
