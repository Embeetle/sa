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

#ifndef __InclusionStatus_h
#define __InclusionStatus_h

#include <iostream>
#include <bitset>

namespace sa {

  enum InclusionStatus {
    // An excluded entity occurs in excluded and unused source files only.  It
    // will not be compiled and linked with the project's executable when
    // building the project.
    InclusionStatus_excluded,

    // An included entity will be compiled and linked with the project's
    // executable when building the project.
    InclusionStatus_included,

    NumberOfInclusionStatuses
  };
  static const char * const InclusionStatus_names[] = {
    "excluded", "included"
  };
  
  inline std::ostream &operator<<(std::ostream &out, InclusionStatus status)
  {
    return out << InclusionStatus_names[status];
  }

  typedef std::bitset<NumberOfInclusionStatuses> InclusionStatusSet;
}

#endif
