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

#ifndef __Constructor_h
#define __Constructor_h

#include "GlobalSymbol.h"
#include "Record.h"

namespace sa {

  class Constructor: public Symbol
  {
    Record * _record;

  public:
    
    Constructor(
      Record *record,
      const std::string &name,
      const std::string &link_name
    );

    ~Constructor();

    Record *record() const { return _record; }
  };

}

#endif

