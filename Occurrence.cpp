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

#include "Occurrence.h"
#include "File.h"
#include "Symbol.h"
#include "Hdir.h"
#include "Diagnostic.h"
#include "Project.h"
#ifdef CHECK
#include "Unit.h"
#endif
#include "base/debug.h"

#if 0
static sa::EntityKind get_effective_entity_kind(
  sa::EntityKind ekind,
  sa::OccurrenceStyle style
)
{
  if (ekind == sa::EntityKind_global_symbol) {
    switch (style) {
      case sa::OccurrenceStyle_unspecified:
      default:
        return sa::EntityKind_global_symbol;
      case sa::OccurrenceStyle_data:
        return sa::EntityKind_global_variable;
      case sa::OccurrenceStyle_function:
        return sa::EntityKind_global_function;
      case sa::OccurrenceStyle_virtual_function:
        return sa::EntityKind_virtual_function;
    }
  }
  return ekind;
}
#endif

sa::Occurrence::Occurrence(
  OccurrenceKind kind,
  OccurrenceStyle style,
  base::ptr<Entity> entity,
  base::ptr<File> file,
  const Range &range
)
  : entity(entity)
  , file(file)
  , kind(kind)
  , style(style)
#if 0
  , ekind(get_effective_entity_kind(entity->kind, style))
#endif
  , range(range)
{
  assert(is_valid(entity));
  assert(is_valid(file));
  trace("create " << this << " " << *this);
  assert_(!instance_count, *this);
  trace("Created " << *this);
}

sa::Occurrence::~Occurrence()
{
  prefail_dump("~Occurrence", Occurrence&, *this);
  trace_nest("delete " << this << " " << *this);
  assert_(!instance_count, *this);
  // Inclusions should drop themselves while they still have an hdir.
  // TODO: make SymRef class avoid test below.
  if (kind != OccurrenceKind_include) {
    assert(file->project->is_locked());
    file->drop_occurrence(this);
  }
  assert(base::is_valid(entity));
  assert(base::is_valid(file));
}

bool sa::Occurrence::can_be_scope() const {
  if (is_definition(kind)) {
    return definition_can_be_scope(entity->kind);
  } else if (is_declaration(kind)) {
    return declaration_can_be_scope(entity->kind);
  } else {
    return false;
  }
}

std::ostream &sa::operator<<(std::ostream &out, const Occurrence &occurrence)
{
  assert(base::is_valid(&occurrence));
  occurrence.write(out);
  return out;
}

void sa::Occurrence::write(std::ostream &out) const
{
  assert(base::is_valid(this));
  assert(base::is_valid(entity));
  assert(base::is_valid(file));
  out << kind << " of " << entity->kind
      << " " << entity->get_entity_name()
      << " at " << file->get_name() << "@" << range;
  Occurrence *scope = entity->get_ref_scope();
  assert_(base::is_valid_or_null(scope), "bad scope");
  if (scope) {
    out << " in " << *scope;
  }
}

base::ptr<sa::Diagnostic> sa::Occurrence::get_diagnostic(
  const std::string &message,
  Severity severity
)
{
  return file->get_diagnostic(message, severity, range.begin);
}

void sa::Occurrence::insert_instance(bool linked)
{
  trace_nest("insert occurrence instance#" << (instance_count+1) << " @"
    << linked << " " << *this
  );
  bool update_required = instance_count && linked && !linked_instance_count;
  // First update entity, then file, to avoid spurious updates.  Updating the
  // entity might change its effective kind, which is relevant when it is
  // tracked in the file.
  entity->insert_instance_of_entity(linked, this, !instance_count,
    linked && !linked_instance_count
  );
  if (!instance_count) {
    file->insert_occurrence_in_file(this);
    if (get_hdir()) {
      get_hdir()->insert_occurrence(this);
    }
  }
  if (linked) {
    if (get_hdir()) {
      get_hdir()->insert_linked_instance(this);
    }
    linked_instance_count++;
    assert(linked_instance_count);
  }
  if (update_required) {
    file->update_occurrence_in_file(this);
  }
  instance_count++;
  assert(instance_count);
}

void sa::Occurrence::remove_instance(bool linked)
{
  trace_nest("remove occurrence instance#" << instance_count << " @" << linked
    << " " << *this
  );
  assert(instance_count);
  instance_count--;
  if (linked) {
    assert(linked_instance_count);
    linked_instance_count--;
    if (get_hdir()) {
      get_hdir()->remove_linked_instance(this);
    }
  }
  // First update file, then entity, to avoid spurious updates.  Updating the
  // entity might change its effective kind, which is relevant when it is
  // tracked in the file.
  if (!instance_count) {
    if (get_hdir()) {
      get_hdir()->remove_occurrence(this);
    }
    file->remove_occurrence_in_file(this);
  }
  entity->remove_instance_of_entity(linked, this, !instance_count,
    linked && !linked_instance_count
  );
  bool update_required = instance_count && linked && !linked_instance_count;
  if (update_required) {
    file->update_occurrence_in_file(this);
  }
}

base::ptr<sa::Hdir> sa::Occurrence::get_hdir() const
{
  return 0;
}

#ifndef NDEBUG
#include <sstream>

std::string sa::Occurrence::get_debug_name() const
{
  std::stringstream buf;
  buf << *this;
  return buf.str();
}
#endif
