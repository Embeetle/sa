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

#include "VirtualFunction.h"
#include <algorithm>

sa::VirtualFunction::VirtualFunction(
  const SymbolUid &uid,
  const std::string &name,
  Project *project,
  Record *parent,
  std::vector<VirtualFunction*> overridees
)
  : Symbol(uid, name, project)
  , _parent(parent)
  , _overridees(overridees)
{
  parent->_virtual_functions.push_back(this);
  for (auto overridee: _overridees) {
    overridee->_overriders.push_back(this);
  }
}

sa::VirtualFunction::~VirtualFunction()
{
  assert(_overriders.empty());
  for (auto overridee: _overridees) {
    overridee->_overriders.erase(
      std::find(
        overridee->_overriders.begin(),
        overridee->_overriders.end(),
        this
      )
    );
  }
  _parent->_virtual_functions.erase(
    std::find(
      _parent->_virtual_functions.begin(),
      _parent->_virtual_functions.end(),
      this
    )
  );
}
