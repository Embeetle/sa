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

#ifndef __sa_status_map_type_h
#define __sa_status_map_type_h

#include "LinkStatus.h"
#include "base/compare_c_strings.h"
#include <map>

namespace sa {
  // Type of link status map for global symbols. Used to report linker results
  // to the project.
  //
  // The key for this map is a C string; there is no need to copy the name of
  // global symbols into a std::string.
  //
#if 0
  typedef std::map<const char *, LinkStatus, base::compare_c_strings>
  link_status_map_type;
#else
  typedef std::map<std::string, LinkStatus> link_status_map_type;
#endif
}

#endif
