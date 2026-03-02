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

#ifndef __base_Chain_h
#define __base_Chain_h

#include "debug.h"

namespace base {

  template <typename T> struct Chain {
    // A chain of objects of a derived class T.
    //
    // A chain is a circular doubly-linked list, with one "sentinel"
    // representing the start and end point of the chain and through which the
    // chain can be accessed.

    // Next and previous links in the chain. Both null for an unconnected chain
    // link.
    T *next;
    T *prev;

    // Create a single unconnected chain link.
    Chain(): next(0), prev(0) {}

    // Create a new chain. The chain created by this constructor contains a
    // single link, created by this constructor. The idea is to use it as a
    // sentinel, with values appropriate for the derived class T.
    Chain(T *t): next(t), prev(t) { assert(t==this); }

    // Return true iff this chain link is connected to a chain.
    bool is_linked()
    {
      return next;
    }

    // Insert this unconnected chain link in a chain after an existing link.
    // An attempt to insert a link that is already part of a chain fails with an
    // assertion.
    void insert_after(T *after)
    {
      assert(!next);
      assert(!prev);
      assert(after->next);
      assert(after->prev);
      next = after->next;
      prev = after;
      next->prev = static_cast<T*>(this);
      prev->next = static_cast<T*>(this);
    }

    // Insert this unconnected chain link in a chain before an existing link.
    // An attempt to insert a link that is already part of a chain fails with an
    // assertion.
    void insert_before(T *before)
    {
      assert(before->prev);
      insert_after(before->prev);
    }

    // Remove this connected chain link from its chain.  An attempt to remove a
    // link that is not part of a chain fails with an assertion.
    void remove()
    {
      assert(next);
      assert(prev);
      next->prev = prev;
      prev->next = next;
      debug_code(next = 0);
      debug_code(prev = 0);
    }
  };
}

#endif
