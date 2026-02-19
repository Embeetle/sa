// Copyright 2018-2024 Johan Cockx
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
