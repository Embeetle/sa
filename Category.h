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

#ifndef __Category_h
#define __Category_h

#include <iostream>
#include <string>
#include <cstddef>

namespace sa {
  // Diagnostic category enum
  typedef enum {
    Category_none,
    Category_toolchain,
    Category_makefile,
  } Category;

  static const size_t NumberOfCategories = 2;

  static const char * const Category_names[] = {
    "none",
    "toolchain",
    "makefile",
  };

  inline Category category_by_name(const std::string &name) {
    for (size_t category = 0; category < NumberOfCategories; ++category) {
      if (Category_names[category] == name) {
        return static_cast<Category>(category);
      }
    }
    return Category_none;
  }

  inline std::ostream &operator<<(std::ostream &out, sa::Category category)
  {
    return out << Category_names[category];
  }
}

#endif
