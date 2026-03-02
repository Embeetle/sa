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

#ifndef __FileMode_h
#define __FileMode_h

namespace sa {
  // Mode enum for source files.
  enum FileMode {
    // A source file in excluded mode is never included in the project's
    // executable when building the project; that is, unless it is included in
    // another source file, e.g. by the preprocessor.
    FileMode_exclude,

    // A source file in included mode is always included in the project's
    // executable when building the project.
    FileMode_include,

    // For a source file in automatic mode, the source analyzer will determine
    // automatically whether it should be included or not in the project's
    // executable when building the project.
    FileMode_automatic,
  };
  static const char * const FileMode_names[] = {
    "exclude", "include", "automatic"
  };

  inline std::ostream &operator<<(std::ostream &out, FileMode mode)
  {
    return out << FileMode_names[mode];
  }
};

#endif
