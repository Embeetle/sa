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

#ifndef __base_index_h
#define __base_index_h

#include <map>
#include <string.h>

namespace base {

  // An index is a collected of objects with a name,  that can be found quickly
  // based on their name.
  //
  // The type of the objects in the collection is a template parameter.  The
  // objects must have a method 'const char *name_data() const' that returns the
  // name as a C-string.
  //
  // Similar functionality can be achieved by using a
  // std::map<std::string,Named*>,  at the cost of duplicating the name.
  template <class Named>
  class index
  {
    struct compare {
      bool operator()(char const *a, char const *b) const {
        return strcmp(a, b) < 0;
      }
    };
    std::map<const char*, Named*, compare> map;
  public:
    Named *find(const char *name) const
    {
      auto it = map.find(name);
      return it == map.end() ? 0 : it->second;
    }
    Named *add(Named *named)
    {
      map[named->name_data()] = named;
    }
    void remove(Named *named)
    {
      map.erase(named->name_data());
    }
  };

}
