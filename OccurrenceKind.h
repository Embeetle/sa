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

#ifndef __OccurrenceKind_h
#define __OccurrenceKind_h

#include <bitset>
#include <iostream>

namespace sa {
  // Symbol occurrence kind enum.
  //
  // This enum expresses differences between occurrences of the same symbol.
  // Properties that are intrinsic to the symbol are expressed by the EntityKind
  // enum.
  enum OccurrenceKind {
    // Order is carefully managed to allow sensible NumberOf... definitions
    OccurrenceKind_definition,                               // D 0 strong def
    OccurrenceKind_tentative_definition,                     // C 1 common def
    OccurrenceKind_weak_definition,                          // W 2 weak def
    OccurrenceKind_declaration,                              // d 3 decl
    OccurrenceKind_weak_declaration,                         // w 4 weak decl
    OccurrenceKind_use,                                      // u 5 
    OccurrenceKind_weak_use,                                 // v 6
    OccurrenceKind_include,                                  // i 7
    
    // Special value to indicate no occurrence
    OccurrenceKind_none,

    // First occurrence kind,  for iteration
    FirstOccurrenceKind = OccurrenceKind_definition,

    // Number of kinds for occurrences declaring a symbol.
    NumberOfDefiningOccurrenceKinds = OccurrenceKind_declaration,

    // Number of kinds for occurrences declaring a symbol.
    NumberOfDeclaringOccurrenceKinds = OccurrenceKind_use,

    // Number of occurrence kinds used for symbols
    NumberOfSymbolOccurrenceKinds = OccurrenceKind_include,

    // Number of occurrence kinds used for entities (symbols and files)
    NumberOfOccurrenceKinds = OccurrenceKind_none, 
  };
  
  static const char * const OccurrenceKind_names[] = {
    "definition", "tentative definition", "weak definition", "declaration",
    "weak declaration", "use", "weak use", "include", "none"
  };

  inline bool is_definition(OccurrenceKind kind)
  {
    return kind <= OccurrenceKind_weak_definition;
  }

  // A definition also counts as a declaration. To eliminate definitions, use
  // is_declaration(kind) && !is_definition(kind)
  inline bool is_declaration(OccurrenceKind kind)
  {
    return kind <= OccurrenceKind_weak_declaration;
  }

  typedef std::bitset<NumberOfOccurrenceKinds> OccurrenceKindSet;

  inline std::ostream &operator<<(std::ostream &out, sa::OccurrenceKind kind)
  {
    return out << sa::OccurrenceKind_names[kind];
  }
}

#endif
