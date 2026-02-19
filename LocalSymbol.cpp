// Copyright 2018-2024 Johan Cockx
#include "LocalSymbol.h"
#include "Project.h"

sa::LocalSymbol::LocalSymbol(
  EntityKind kind,
  const std::string &name,
  const FileLocation &ref_location,
  base::ptr<Occurrence> ref_scope,
  Project *project
)
  : Symbol(kind, name, project)
  , ref_location(ref_location)
  , ref_scope(ref_scope)
{
  trace("Create LocalSymbol " << *this);
  assert(!is_global());
  assert(base::is_valid_or_null(ref_scope));
}

sa::LocalSymbol::~LocalSymbol()
{
  assert(base::is_valid_or_null(ref_scope));
  assert(base::is_valid(ref_location.file));
}

sa::LocalSymbol *sa::LocalSymbol::as_local_symbol()
{
  return this;
}

void sa::LocalSymbol::zero_ref_count()
{
  assert(project->is_locked());
  trace("remove symbol " << *this);
  project->erase_local_symbol(this);
  Entity::zero_ref_count();
}

bool sa::LocalSymbol::operator<(const LocalSymbol& other) const
{
  assert(base::is_valid_or_null(ref_scope));
  assert(base::is_valid_or_null(other.ref_scope));
  bool smaller = name != other.name ? name < other.name :
    kind != other.kind ? kind < other.kind :
    ref_location != other.ref_location
                                ? ref_location < other.ref_location :
    ref_scope < other.ref_scope
            ;
  trace(this << " " << *this << " < " << &other << " " << other << " : "
    << smaller
  );
  return smaller;
}

bool sa::LocalSymbol::operator==(const LocalSymbol& other) const
{
  assert(base::is_valid_or_null(ref_scope));
  assert(base::is_valid_or_null(other.ref_scope));
  return name == other.name
    && kind == other.kind 
    && ref_location == other.ref_location
    && ref_scope == other.ref_scope 
    ;
}

void sa::LocalSymbol::write(std::ostream &out) const
{
  out << *this;
}

std::ostream &sa::operator<<(std::ostream &out, const LocalSymbol &symbol)
{
  assert(base::is_valid_or_null(symbol.ref_scope));
  out << symbol.name << ": " << symbol.kind
      << " first seen at " << symbol.get_ref_location();
  if (symbol.ref_scope) {
    out << " in scope of " << *symbol.ref_scope;
  }
  return out;
}

sa::Occurrence *sa::LocalSymbol::get_ref_scope() const
{
  assert(base::is_valid_or_null(ref_scope));
  return ref_scope;
}
