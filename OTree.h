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

#ifndef __sa_OTree_h
#define __sa_OTree_h

#include "Range.h"
#include "OccurrenceKind.h"
#include "base/ptr.h"
#include <set>

namespace sa {
  class Occurrence;
  class File;
  class Entity;
  class Hdir;

  // A container for occurrences, supporting efficient O(log n):
  // - lookup of all occurrences overlapping a given range
  // - lookup or insertion of a specific occurrence
  // - edit of the underlying source file,  making some occurrences unreachable
  //   and others shift location
  // - removal of a given occurrence
  //
  // The container does not manage concurrent access; handling concurrent access
  // is the responsibility of the user.  Use a mutex or similar if necessary.
  //
  class OTree {
  public:

    // Find occurrences overlapping the given range and add them to the set.
    // The given range is post-edit.
    void lookup(Range range, std::set<Occurrence*> &found) const;

    

    // Find existing or insert new occurrence with the given properties.  The
    // given range is post-edit.
    base::ptr<Occurrence> get(
      Range range, 
      File *file,
      const base::ptr<Entity> &entity,
      OccurrenceKind kind,
      const base::ptr<Hdir> hdir
    );

    // Patch occurrence locations for replacement of the range by new text.
    void edit(Range range, const char *new_text);

    // Drop the given occurrence (because it no longer occurs in any compilation
    // unit).
    void drop(Occurrence *occurrence);
  };
}

#endif
