// Copyright 2018-2024 Johan Cockx
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
