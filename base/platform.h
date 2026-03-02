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

#ifndef __base_platform_h
#define __base_platform_h

#include <string>
#include <typeinfo>

namespace base {
  std::string demangle(const char *name);
  inline std::string demangle(const std::string &name)
  {
    return demangle(name.data());
  }
  
  template <typename T> std::string type_name(const T &t)
  {
    return demangle(typeid(t).name());
  }

}

#endif
