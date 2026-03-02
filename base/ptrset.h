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

#ifndef __base_ptrset_h
#define __base_ptrset_h

#include <set>

namespace base {

  // Set of pointers or pointer-like objects to comparable objects.  Comparison
  // is done on the objects, not the pointers. A ptrset will never contain
  // pointers to objects that compare equal, even if the pointers are different.
  // 
  template <class P>
  class ptrset {
    static bool compare(P item1, P item2)
    {
      return *item1 < *item2;
    }

    std::set<P, decltype(&compare)> set;

  public:
    ptrset(): set(compare) {}

    bool empty() const { return set.empty(); }

    void insert(P item)
    {
      set.insert(item);
    }

    void erase(P item)
    {
      auto it = set.find(item);
      if (*it == item) {
        set.erase(it);
      }
    }

    P find(P item, P notfound = 0) const
    {
      auto it = set.find(item);
      return it==set.end() ? notfound : *it;
    }
  
    P find_or_insert(P &&item)
    {
      auto it = set.find(item);
      if (it==set.end()) {
        insert(item);
        return item;
      } else {
        return *it;
      }
    }

    typename std::set<P, decltype(&compare)>::iterator begin()
    {
      return set.begin();
    }

    typename std::set<P, decltype(&compare)>::iterator end()
    {
      return set.end();
    }

    void swap(ptrset &other)
    {
      set.swap(other.set);
    }
  };
  
}
    
#endif
