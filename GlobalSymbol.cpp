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

#include "GlobalSymbol.h"
#include "Project.h"
#include "base/platform.h"

sa::GlobalSymbol::GlobalSymbol(
  const std::string &link_name,
  Project *project
)
  : Symbol(EntityKind_global_symbol, base::demangle(link_name), project)
  , link_name(link_name)
{
  trace("create GlobalSymbol " << this << " " << *this);
}

sa::GlobalSymbol::~GlobalSymbol()
{
  trace("destroy GlobalSymbol " << this << " " << *this);
  // Note: link status will differ from none when there are occurrences with
  // the same global name, so don't assert(link_status == LinkStatus_none)
  assert(!get_main_occurrence());
}

sa::GlobalSymbol *sa::GlobalSymbol::as_global_symbol()
{
  return this;
}

void sa::GlobalSymbol::add_undefined_diagnostic(Occurrence *use)
{
  trace("add undefined diagnostic for " << use);
  std::ostringstream message;
  message << "undefined " << kind << " " << name;
  add_diagnostic(use, message.str(), Severity_error);
}

void sa::GlobalSymbol::add_multiply_defined_diagnostic(Occurrence *definition)
{
  trace("add multiply defined diagnostic for " << definition);
  std::ostringstream message;
  message << "multiply defined " << kind << " " << name;
  add_diagnostic(definition, message.str(), Severity_error);
}

void sa::GlobalSymbol::insert_instance_of_entity(
  bool linked,
  Occurrence *occurrence,
  bool is_first_instance,
  bool is_first_linked_instance
)
{
  trace_nest("GlobalSymbol::insert_instance " << linked << " " << *occurrence
    << " " << link_name << " as " << occurrence->style
    << " " << is_first_instance << "/" << is_first_linked_instance
  );
  trace("link status: " << link_status);
  Symbol::insert_instance_of_entity(
    linked, occurrence, is_first_instance, is_first_linked_instance
  );
  if (is_first_linked_instance) {
    // This occurrence is now linked, so any link related error becomes
    // relevant. Whether there is a link-related error depends on the link
    // status, which is defined elsewhere.
    switch (link_status) {
      case LinkStatus_undefined: {
        if (occurrence->kind == OccurrenceKind_use) {
#ifndef SUPPRESS_UNDEFINED_DIAGNOSTICS
          // Temporarily suppress undefined global diagnostics
          add_undefined_diagnostic(occurrence);
#endif
        }
        break;
      }
      case LinkStatus_multiply_defined: {
        if (occurrence->kind == OccurrenceKind_definition) {
          add_multiply_defined_diagnostic(occurrence);
        }
        break;
      }
      default: {
        break;
      }
    }
  }
  {
    // Check the effect of this occurrence on the effective entity kind.
    // Currently, do this even if the occurrence is not linked yet, to increase
    // the probablity to get the correct effective kind: later changes in
    // effective kind are not yet taken into account for tracked occurrences.
    bool update = false;
    switch (occurrence->style) {
      case OccurrenceStyle_data:
        if (!data_count) update = true;
        ++data_count;
        assert(data_count);
        break;
      case OccurrenceStyle_function:
        if (!function_count) update = true;
        ++function_count;
        assert(function_count);
        break;
      case OccurrenceStyle_virtual_function:
        if (!virtual_function_count) update = true;
        ++virtual_function_count;
        assert(virtual_function_count);
        break;
      default:
        break;
    }
    if (update) {
      recompute_effective_kind();
    }
  }
}

void sa::GlobalSymbol::remove_instance_of_entity(
  bool linked,
  Occurrence *occurrence,
  bool is_last_instance,
  bool is_last_linked_instance
)
{
  trace_nest("GlobalSymbol::remove_instance " << linked << " " << *occurrence
    << " " << link_name << " as " << occurrence->style
    << " " << is_last_instance << "/" << is_last_linked_instance
  );
  trace("link status: " << link_status);
  {
    // Check the effect of this occurrence on the effective entity kind.
    // Currently, do this even if the occurrence is not linked yet, to increase
    // the probablity to get the correct effective kind: later changes in
    // effective kind are not yet taken into account for tracked occurrences.
    bool update = false;
    switch (occurrence->style) {
      case OccurrenceStyle_data:
        assert(data_count);
        --data_count;
        if (!data_count) update = true;
        break;
      case OccurrenceStyle_function:
        assert(function_count);
        --function_count;
        if (!function_count) update = true;
        break;
      case OccurrenceStyle_virtual_function:
        assert(virtual_function_count);
        --virtual_function_count;
        if (!virtual_function_count) update = true;
        break;
      default:
        break;
    }
    if (update) {
      recompute_effective_kind();
    }
  }
  if (is_last_linked_instance) {
    // This occurrence is no longer linked, so any link related error becomes
    // irrelevant. Whether there is a link-related error depends on the link
    // status, which is defined elsewhere.
    switch (link_status) {
      case LinkStatus_undefined: {
        if (occurrence->kind == OccurrenceKind_use) {
#ifndef SUPPRESS_UNDEFINED_DIAGNOSTICS
          // Temporarily suppress undefined global diagnostics
          remove_diagnostic(occurrence);
#endif
        }
        break;
      }
      case LinkStatus_multiply_defined: {
        if (occurrence->kind == OccurrenceKind_definition) {
          remove_diagnostic(occurrence);
        }
        break;
      }
      default: {
        break;
      }
    }
  }
  Symbol::remove_instance_of_entity(
    linked, occurrence, is_last_instance, is_last_linked_instance
  );
}

void sa::GlobalSymbol::recompute_effective_kind()
{
  EntityKind new_kind;
  if (data_count && !function_count && !virtual_function_count) {
    new_kind = EntityKind_global_variable;
  } else if (!data_count && function_count && !virtual_function_count) {
    new_kind = EntityKind_global_function;
  } else if (!data_count && !function_count && virtual_function_count) {
    new_kind = EntityKind_virtual_function;
  } else {
    new_kind = EntityKind_global_symbol;
  }
  if (new_kind != effective_kind) {
    // TODO: refresh tracked occurrences
    effective_kind = new_kind;
  }
  trace("effective kind -> " << effective_kind);
}

void sa::GlobalSymbol::set_link_status(LinkStatus status)
{
  trace_nest("set link status of " << *this << " "
    << link_status << " --> " << status
  );
  assert(project->is_locked());
  if (link_status != status) {
    trace("change link status of " << *this << " "
      << link_status << " --> " << status
    );
    switch (link_status) {
      case LinkStatus_undefined: {
#ifndef SUPPRESS_UNDEFINED_DIAGNOSTICS
        for (auto use: uses) {
          if (use->is_linked()) {
            remove_diagnostic(use);
          }
        }
        // For virtual functions, a declaration is an implicit use, because it
        // adds the function to the vtable. A virtual function can be undefined
        // even if it is never called directly.
        if (effective_kind == EntityKind_virtual_function) {
          for (auto def: defs) {
            if (def->is_linked()) {
              remove_diagnostic(def);
            }
          }
        }
#endif
        break;
      }
      case LinkStatus_multiply_defined: {
        for (auto def: defs) {
          if (def->kind == OccurrenceKind_definition) {
            if (def->is_linked()) {
              remove_diagnostic(def);
            }
          }
        }
        break;
      }
      default: {
        break;
      }
    }
    link_status = status;
    switch (link_status) {
      case LinkStatus_undefined: {
#ifndef SUPPRESS_UNDEFINED_DIAGNOSTICS
        for (auto use: uses) {
          if (use->is_linked()) {
            // Temporarily suppress undefined global diagnostics
            add_undefined_diagnostic(use);
          }
        }
        // For virtual functions, a declaration is an implicit use, because it
        // adds the function to the vtable. A virtual function can be undefined
        // even if it is never called directly.
        if (effective_kind == EntityKind_virtual_function) {
          for (auto def: defs) {
            if (def->is_linked()) {
              add_undefined_diagnostic(def);
            }
          }
        }
#endif
        break;
      }
      case LinkStatus_multiply_defined: {
        for (auto def: defs) {
          if (def->kind == OccurrenceKind_definition) {
            if (def->is_linked()) {
              add_multiply_defined_diagnostic(def);
            }
          }
        }
        break;
      }
      default: {
        break;
      }
    }
  }
}

void sa::GlobalSymbol::zero_ref_count()
{
  assert(project->is_locked());
  trace_nest("zero ref count -> remove global symbol " << *this);
  project->erase_global_symbol(this);
  Entity::zero_ref_count();
}

void sa::GlobalSymbol::write(std::ostream &out) const
{
  out << *this;
}

std::ostream &sa::operator<<(std::ostream &out, const GlobalSymbol &symbol)
{
  out << symbol.name << ": " << symbol.kind << " " << symbol.link_name;
  if (symbol.get_link_status() != LinkStatus_none) {
    out << " link-status:" << symbol.get_link_status();
  }
  return out;
}


