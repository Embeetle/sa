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

#ifndef __base_compare_c_strings_h
#define __base_compare_c_strings_h

#include <string.h>

namespace base {

  // Aux object to compare C strings, usable as third template parameter for
  // std::map and std::set.

  struct compare_c_strings {
    bool operator()(char const *a, char const *b) const {
      return strcmp(a, b) < 0;
    }
  };
}

#endif
