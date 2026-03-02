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

#ifndef __Severity_h
#define __Severity_h

#include <iostream>
#include <string>
#include <cstddef>

namespace sa {
  // Diagnostic severity enum
  typedef enum {
    // Diagnostic indicates suspicious code that may not be wrong.
    Severity_warning,

    // Diagnostic indicates that the code is ill-formed.
    Severity_error,

    // Diagnostic indicates that analysis failed.
    Severity_fatal,

    // Internal value,  never used for actual diagnostic
    Severity_none,
  } Severity;

  static const char * const Severity_names[] = {
    "warning", "error", "fatal", "none"
  };

  static const size_t NumberOfSeverities = Severity_none;

  inline Severity severity_by_name(const std::string &name) {
    for (int severity = Severity_warning; severity < Severity_none; ++severity) {
      if (Severity_names[severity] == name) {
        return static_cast<Severity>(severity);
      }
    }
    return Severity_none;
  }

  inline std::ostream &operator<<(std::ostream &out, sa::Severity severity)
  {
    return out << Severity_names[severity];
  }
}

#endif
