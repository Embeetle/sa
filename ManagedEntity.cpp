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

#include "ManagedEntity.h"

//------------------------------------------------------------------------------
// For background on atomics and memory order, see comments in base/RefCounted.h
//------------------------------------------------------------------------------

sa::ManagedEntity::~ManagedEntity()
{
  trace("destroy entity");
  assert(!is_known());
}

bool sa::ManagedEntity::is_known() const {
  return known_count.load(std::memory_order_relaxed);
}


void sa::ManagedEntity::increment_known()
{
  trace("increment known for " << get_entity_name());
  auto known = known_count.fetch_add(1, std::memory_order_relaxed);
  if (!known) {
    trace("mark known");
    increment_ref_count();
    assert(!get_user_data());
    create_user_data();
  }
}

void sa::ManagedEntity::decrement_known()
{
  trace("decrement known for " << get_entity_name());
  auto known = known_count.fetch_sub(1, std::memory_order_acq_rel);
  if (known == 1) {
    trace("mark unknown");
    delete_user_data();
    set_user_data(0);
    decrement_ref_count();
  }
}
