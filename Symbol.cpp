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

#include "Symbol.h"
#include "Project.h"
#include "File.h"
#include "Diagnostic.h"
#include "base/debug.h"
#include <sstream>

sa::Symbol::Symbol(
  EntityKind kind,
  const std::string &name,
  Project *project
)
  : ManagedEntity(kind, project)
  , name(name)
{
  trace("create symbol " << kind << " " << name);
}

sa::Symbol::~Symbol()
{
  trace("destroy symbol " << *this);
  assert(uses.empty());
  assert(defs.empty());
  assert(!is_known());
}

sa::Symbol *sa::Symbol::as_symbol()
{
  return this;
}

void sa::Symbol::create_user_data()
{
  project->add_symbol(this);
}

void sa::Symbol::delete_user_data()
{
  project->drop_symbol(this);
}
  
base::ptr<sa::Occurrence> sa::Symbol::get_main_occurrence() const
{
  assert(project->is_locked());
  if (!defs.empty()) {
    base::ptr<sa::Occurrence> occurrence = defs.at(0);
    for (auto def: defs) {
      if (def->kind < occurrence->kind) {
        occurrence = def;
      }
    }
    return occurrence;
  } else if (!uses.empty()) {
    return uses.at(0);
  } else {
    return 0;
  }
}

bool sa::Symbol::has_definition() const
{
  return !defs.empty();
}

std::vector<base::ptr<sa::Occurrence>> sa::Symbol::get_occurrences(
  OccurrenceKind kind
)
{
  assert(project->is_locked());
  std::vector<base::ptr<Occurrence>> occurrence_list;
  if (kind == OccurrenceKind_use) {
    for (auto use: uses) {
      occurrence_list.push_back(use);
    }
  } else {
    for (auto def: defs) {
      if (def->kind == kind) {
        occurrence_list.push_back(def);
      }
    }
  }
  return occurrence_list;
}

void sa::Symbol::insert_instance_of_entity(
  bool linked,
  Occurrence *occurrence,
  bool is_first_instance,
  bool is_first_linked_instance
)
{
  trace_nest("Symbol::insert_instance " << linked << " " << *occurrence
    << " " << is_first_instance << "/" << is_first_linked_instance
  );
  assert(project->is_locked());
  assert(!occurrence || occurrence->entity == this);
  if (is_first_instance) {
    assert(occurrence);
    OccurrenceKind kind = occurrence->kind;
    assert(kind < NumberOfSymbolOccurrenceKinds);
    if (kind == OccurrenceKind_use) {
      uses.insert(occurrence);
    } else {
      defs.insert(occurrence);
    }
    if (tracked_occurrence_kinds[kind]) {
      project->add_occurrence_of_entity(occurrence);
    }
  }
}

void sa::Symbol::remove_instance_of_entity(
  bool linked,
  Occurrence *occurrence,
  bool is_last_instance,
  bool is_last_linked_instance
)
{
  trace_nest("Symbol::remove_instance " << linked << " " << *occurrence
    << " " << is_last_instance << "/" << is_last_linked_instance
  );
  assert(project->is_locked());
  assert(!occurrence || occurrence->entity == this);
  if (is_last_instance) {
    assert(occurrence);
    OccurrenceKind kind = occurrence->kind;
    assert(kind < NumberOfSymbolOccurrenceKinds);
    if (tracked_occurrence_kinds[kind]) {
      project->remove_occurrence_of_entity(occurrence);
    }
    if (kind == OccurrenceKind_use) {
      uses.remove(occurrence);
    } else {
      defs.remove(occurrence);
    }
  }
}

void sa::Symbol::add_diagnostic(
  Occurrence *occurrence,
  const std::string &message,
  Severity severity
)
{
  assert(is_valid(occurrence));
  auto diagnostic = occurrence->get_diagnostic(message, severity);
  project->add_link_diagnostic(occurrence, diagnostic);
}

void sa::Symbol::remove_diagnostic(Occurrence *occurrence)
{
  assert(is_valid(occurrence));
  project->remove_link_diagnostic(occurrence);
}

void sa::Symbol::track_occurrences_of_entity(
  OccurrenceKindSet occurrence_kinds
)
{
  trace_nest("track occurrences " << occurrence_kinds
    << " of " << get_entity_name()
  );
  assert(project->is_locked());
  for (unsigned okind = NumberOfSymbolOccurrenceKinds; okind--; ) {
    const bool track_okind_old = tracked_occurrence_kinds[okind];
    const bool track_okind_new = occurrence_kinds[okind];
    if (track_okind_old != track_okind_new) {
      if (track_okind_new) {
        trace(" `-> add " << (OccurrenceKind)okind << "s");
        if (okind == OccurrenceKind_use) {
          for (auto use: uses) {
            project->add_occurrence_of_entity(use);
          }
        } else {
          for (auto def: defs) {
            if (def->kind == okind) {
              project->add_occurrence_of_entity(def);
            }
          }
        }
      } else {
        trace(" `-> remove " << (OccurrenceKind)okind << "s");
        if (okind == OccurrenceKind_use) {
          for (auto use: uses) {
            project->remove_occurrence_of_entity(use);
          }
        } else {
          for (auto def: defs) {
            if (def->kind == okind) {
              project->remove_occurrence_of_entity(def);
            }
          }
        }
      }
    }
  }
  tracked_occurrence_kinds = occurrence_kinds;
}
