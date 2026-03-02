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

#ifndef __LinkStatus_h
#define __LinkStatus_h

#include <iostream>

namespace sa {

  // Link status for a symbol.
  enum LinkStatus {
    // The symbol is not involved in linking, either because it is not directly
    // or indirectly used from any of the program's entry points, or because it
    // is a local symbol.
    LinkStatus_none,

    // The symbol is undefined and only weakly used; if no definition is found,
    // it defaults to zero without error.
    LinkStatus_weakly_undefined,

    // The symbol is used and has no definitions
    LinkStatus_undefined,

    // The symbol is used, has no strong or tentative definition and has one or
    // more weak definitions.
    LinkStatus_weakly_defined,

    // The symbol is used, has no strong definition and has one or more tentative
    // (=common) definitions.
    LinkStatus_tentatively_defined,

    // The symbol is used and has exactly one strong definition
    LinkStatus_defined,

    // The symbol is used and has more than one strong definition
    LinkStatus_multiply_defined,
  };
  
  static const char * const LinkStatus_names[] = {
    "none", "weakly undefined", "undefined", "weakly defined",
    "tentatively defined", "defined", "multiply defined"
  };

  inline bool is_defined(LinkStatus status)
  {
    return status > LinkStatus_undefined;
  }

  inline std::ostream &operator<<(std::ostream &out, LinkStatus status)
  {
    return out << LinkStatus_names[status];
  }
}

#endif
