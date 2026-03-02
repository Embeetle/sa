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

#ifndef __VirtualFunction_h
#define __VirtualFunction_h

#include "Symbol.h"
#include "Record.h"

namespace sa {

  class VirtualFunction: public Symbol
  {
    Record * _parent;
    std::vector<VirtualFunction*> _overridees;
    std::vector<VirtualFunction*> _overriders;

  public:
    
    VirtualFunction(
      const SymbolUid &uid,
      const std::string &name,
      Project *project,
      Record *parent,
      std::vector<VirtualFunction*> overridees
    );

    ~VirtualFunction();

    Record *parent() const { return _parent; }
    
    const std::vector<VirtualFunction*> &overridees() const
    {
      return _overridees;
    }
    
    const std::vector<VirtualFunction*> &overriders() const
    {
      return _overriders;
    }
    
  };

}

#endif

