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

#ifndef __sa_Hdir_h
#define __sa_Hdir_h

#include "Lockable.h"
#include "base/RefCounted.h"
#include "base/debug.h"
#include "base/ptr.h"
#include <string>
#include <map>
#include <set>

namespace sa {
  class Occurrence;
  class Project;
  class File;

  // An hdir is a directory from which (header) files are or may be included.
  // Hdirs are created when the application calls Project::add_hdir and when
  // an include statement is found during analysis that uses another hdir.
  // These other hdirs might be hard-coded in the makefile or in the toolchain.
  //
  // Hdirs are reference-counted.  There is one count for addition by the
  // application and one for each include-occurrence. They are automatically
  // removed when the reference count reaches zero.
  class Hdir: public base::RefCounted, public Lockable {
  public:
    // Absolute path of hdir.
    const std::string path;

    // Create an hdir. Path must be absolute and cannot be empty.
    Hdir(Project *project, const std::string &path);

    // Destroy an hdir. Do not call directly if reference counting is used.
    // Make sure the project is locked; otherwise, another thread might attempt
    // to get this hdir while the destructor is running, causing a crash
    // (assertion failure - attempt to increment reference count from zero).
    ~Hdir();

    // Request an update of the compilation flags for all compilation units that
    // rely on this hdir. This will start a background task to re-derive these
    // flags. If the flags have changed, the compilation unit itself will also
    // be re-analyzed.
    void update_flags_for_dependent_units(const char *reason);

    // Insert an occurrence using this hdir. Used to maintain the set of
    // occurrences of this hdir. Call only for the first instance of the
    // occurrence in any compilation unit. Do not call for other instances in
    // the same or another compilation unit. Do not call for occurrences that
    // are not registered in a compilation unit, but only exist in an analyzer.
    void insert_occurrence(Occurrence *occurrence);

    // Remove an occurrence using this hdir. Used to maintain the set of
    // occurrences of this hdir. Call only for the last instance of the
    // occurrence in any compilation unit. Do not call for other instances in
    // the same or another compilation unit. Do not call for occurrences that
    // are not registered in a compilation unit, but only exist in an analyzer.
    void remove_occurrence(Occurrence *occurrence);

    // Insert an instance of an occurrence using this hdir in a linked
    // compilation unit. The first such instance of each occurrence must be
    // inserted. It is allowed but not required to insert additional instances.
    void insert_linked_instance(Occurrence *);
    
    // Remove an instance of an occurrence using this hdir in a linked
    // compilation unit. Only instances that have been inserted and not removed
    // yet can be removed.
    void remove_linked_instance(Occurrence *);

#ifndef NDEBUG
    std::string get_debug_name() const override;
#endif
    
  private:
    Project *project;

    // Set of occurrences using this hdir.
    std::set<Occurrence*> _occurrences;

    // Number of instances of occurrences in linked compilation units.
    size_t _inclusion_count = 0;
  };
}

#endif
