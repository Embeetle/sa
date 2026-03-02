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

#ifndef __Scope_h
#define __Scope_h

namespace sa {

  // A scope with nested occurrences.
  //
  // Not all occurrences have an associated scope, so this data is only stored
  // for those occurrences that have one.  To reduce memory usage, it is not
  // stored in the occurrence object itself but in a lookup table (see
  // Unit::_scope_data).
  //
  struct Scope {
    
    // Index of first nested occurrence in occurrence list. This is one beyond
    // the index of the definition associated with the scope.
    size_t index = 0;

    // Number of nested occurrences. Includes all descendants, not just direct
    // children.
    size_t count = 0;
  };
}

#endif
