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

#ifndef __Section_h
#define __Section_h

#include "OccurrenceKind.h"
#include "base/ptr.h"
#include "base/RefCounted.h"
#include "base/debug.h"
#include <vector>
#include <set>
#include <string>

namespace sa {
  class Occurrence;
  class GlobalSymbol;
  class Unit;

  // A section is a set of linker-relevant occurrences
  class Section: public base::RefCounted
  {
    // Whether this section is linked into the elf file.
    bool _is_linked = false;
    
    // List of occurrences of global symbols in this section, per occurrence
    // kind.
    std::vector<base::ptr<Occurrence>> _global_occurrences[
      NumberOfSymbolOccurrenceKinds
    ];

    // Global symbols for which a definition is required when the code for this
    // section is emitted.
    //
    // Global symbols may be required even if they are not used directly. An
    // example are virtual functions of (a base class of) a class that is
    // instantiated in this section.
    //
    // Global symbols may also not be required even if the section contains a
    // use of the symbol. An example are calls in some inline functions that are
    // themselves never called in the current compilation unit.
    //
    // In other words: the list of required global symbols cannot be derived
    // directly from the list of uses of that symbol. To simplify things, we
    // keep a separate set of required globals.
    std::set<base::ptr<GlobalSymbol>> _required_globals;

    // Weakly required globals are like required globals, except that no error
    // occurrs when a definition is missing; instead, these symbols get a
    // default value (usually zero);
    std::set<base::ptr<GlobalSymbol>> _weakly_required_globals;
    
  public:
    // The unit this section is a part of.  Use a plain pointer, not a
    // ref-counted pointer, to avoid a ref-count loop.
    Unit *const unit;

    std::string const name;

    // For sections from archive files; otherwise empty.
    std::string const member_name;

    Section(
      Unit *unit,
      const std::string &name,
      const std::string &member_name = ""
    );

    const std::vector<base::ptr<Occurrence>> &get_global_occurrences(
      OccurrenceKind kind
    ) const
    {
      return _global_occurrences[kind];
    }

    bool instantiates_occurrence(base::ptr<sa::Occurrence> occurrence);

    const std::set<base::ptr<GlobalSymbol>> &get_required_globals() const
    {
      return _required_globals;
    }

    const std::set<base::ptr<GlobalSymbol>> &get_weakly_required_globals() const
    {
      return _weakly_required_globals;
    }

    void add_occurrence(base::ptr<sa::Occurrence> occurrence);
    
    void require_definition(base::ptr<GlobalSymbol> symbol);

    void weakly_require_definition(base::ptr<GlobalSymbol> symbol);

    bool is_linked() const { return _is_linked; }

    // Mark this section as linked. Propagate changes to the compilation unit.
    void set_linked();

    // Mark this section as unlinked. Propagate changes to the compilation unit.
    void set_unlinked();

    friend std::ostream &operator<<(std::ostream &out, const Section &section);

  protected:
    // Destructor called on zero ref count
    ~Section();

  private:
#ifdef CHECK
    void notify_ref_count() const override;
#endif
  };

  std::ostream &operator<<(std::ostream &out, const Section &section);
}

#endif
