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

#ifndef __base_reverse_h
#define __base_reverse_h

namespace base {
  template <typename T>
  struct reversion_wrapper { T& iterable; };

  template <typename T>
  auto begin (reversion_wrapper<T> w) { return std::rbegin(w.iterable); }

  template <typename T>
  auto end (reversion_wrapper<T> w) { return std::rend(w.iterable); }

  template <typename T>
  reversion_wrapper<T> reverse (T&& iterable) { return { iterable }; }
}

#endif
