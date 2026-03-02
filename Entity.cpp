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

#include "Entity.h"

sa::Entity::Entity(EntityKind kind, Project *project)
  : kind(kind)
  , project(project)
{
  trace("create entity");
}

sa::Entity::~Entity()
{
  trace("destroy entity");
}

void sa::Entity::increment_known()
{
  trace("increment known for " << get_entity_name());
  increment_ref_count();
}

void sa::Entity::decrement_known()
{
  trace("decrement known for " << get_entity_name());
  decrement_ref_count();
}

sa::Symbol *sa::Entity::as_symbol()
{
  assert_(false, kind);
  return 0;
}

sa::GlobalSymbol *sa::Entity::as_global_symbol()
{
  assert_(false, kind);
  return 0;
}

sa::LocalSymbol *sa::Entity::as_local_symbol()
{
  assert_(false, kind);
  return 0;
}

sa::File *sa::Entity::as_file()
{
  assert_(false, kind);
  return 0;
}

std::ostream &sa::operator<<(std::ostream &out, const sa::Entity &entity)
{
  return out << entity.kind << " " << entity.get_name();
}
