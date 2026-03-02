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

#include "platform.h"
#include <string>
#include <cxxabi.h>

std::string base::demangle(const char *name)
{
  if (name[0] != '_' || name[1] != 'Z') return name;
  int status;
  char *demangled_name_ptr = abi::__cxa_demangle(name, 0, 0, &status);
  if (status) return name;
  std::string demangled_name = demangled_name_ptr;
  free(demangled_name_ptr);
  return demangled_name;
}

#ifdef SELFTEST

#include <iostream>

int main()
{
  std::cout << "Selftest for platform\n";
  std::cout << base::demangle("a") << "\n";
  std::cout << "Bye\n";
  return 0;
}

#endif

