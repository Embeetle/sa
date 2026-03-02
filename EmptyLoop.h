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

#ifndef __EmptyLoop_h
#define __EmptyLoop_h

#include "Range.h"
#include "base/ptr.h"
#include "base/RefCounted.h"
#include "base/debug.h"
#include <set>
#include <iostream>

namespace sa {
  class File;
  
  class EmptyLoop: public base::RefCounted {
  
  public:
    // The file and location at which the empty occurs
    const base::ptr<File> file;
    const Range &get_range() const { return range; };

    ~EmptyLoop();

    // Do not call this constructor directly,  but use File::get_empty_loop()!
    EmptyLoop(base::ptr<File> file, const Range &range);

    // Return true iff this empty loop is instantiated in at least one
    // compilation unit. This returns false for empty loops created during
    // source code analysis but not yet reported to the compilation unit. It is
    // possible that an empty loop is never instantiated, for example because
    // the source code analysis is cancelled before it can report its results.
    bool is_instantiated() const
    {
      return instance_count;
    }

    // An empty loop in a header file will be instantiated in each compilation
    // unit that includes the header file. If a compilation unit includes the
    // same header file twice, it will be instantiated twice.
    
    // Insert an instance of this empty loop.
    void insert_instance();

    // Remove an instance of this empty loop.
    void remove_instance();

    // Aux method for operator<<
    virtual void write(std::ostream &out) const;

#ifndef NDEBUG
    std::string get_debug_name() const override;
#endif

  private:
    friend class File;

    Range range;

    // The number of instances of this empty loop in compilation units,
    // excluding instances in analyzers. The instance count controls
    // registration of the empty loop in its entity: it is registered iff the
    // instance count is non-zero.
    //
    // The instance count can be larger than the number of compilation units
    // because a compilation unit may hold more than one instance of the same
    // empty loop, for example if it includes the same file twice.
    size_t instance_count = 0;

  public:
    // Internal field that must be public because empty loops are inserted in a
    // MemberList, which is a set with constant time insertion and removal.
    //
    // Index in file's list of empty loops.
    size_t loop_index = 0;
  };

  std::ostream &operator<<(std::ostream &out, const EmptyLoop &empty_loop);
}

#endif
