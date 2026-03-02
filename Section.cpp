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

#include "Section.h"
#include "Occurrence.h"
#include "GlobalSymbol.h"
#include "Unit.h"
#include "Linker.h"
#include <utility>
#include <algorithm>

sa::Section::Section(
  Unit *unit,
  const std::string &name,
  const std::string &member_name
)
  : unit(unit)
  , name(name)
  , member_name(member_name)
{
  trace("create section " << *this << " linked=" << _is_linked);
}

sa::Section::~Section()
{
  trace_nest("destroy section " << *this << " linked=" << _is_linked);
  if (_is_linked) {
    unit->linker->drop_section(this);
  }
}

bool sa::Section::instantiates_occurrence(base::ptr<sa::Occurrence> occurrence)
{
  auto &occs = _global_occurrences[occurrence->kind];
  return std::find(occs.begin(), occs.end(), occurrence) != occs.end();
}

void sa::Section::add_occurrence(base::ptr<sa::Occurrence> occurrence)
{
  _global_occurrences[occurrence->kind].push_back(occurrence);
}

void sa::Section::require_definition(base::ptr<GlobalSymbol> symbol)
{
  _required_globals.emplace(symbol);
}

void sa::Section::weakly_require_definition(base::ptr<GlobalSymbol> symbol)
{
  _weakly_required_globals.emplace(symbol);
}

void sa::Section::set_linked()
{
  if (!_is_linked) {
    trace_nest("link section " << *this << " refcnt=" << get_ref_count());
    _is_linked = true;
    assert(base::is_valid(unit));
    unit->inc_soft_link_count();
  }
}

void sa::Section::set_unlinked()
{
  if (_is_linked) {
    trace_nest("unlink section " << *this << " refcnt=" << get_ref_count());
    _is_linked = false;
    assert(base::is_valid(unit));
    unit->dec_soft_link_count();
  }
}

std::ostream &sa::operator<<(std::ostream &out, const Section &section)
{
  assert(base::is_valid((Unit*)section.unit));
  out << section.unit->process_name();
  if (!section.member_name.empty()) {
    out << ":" << section.member_name;
  }
  if (!section.name.empty()) {
    out << "@" << section.name;
  }
  return out;
}

#ifdef CHECK
void sa::Section::notify_ref_count() const
{
  trace("Set ref count for section " << *this << " to " << get_ref_count());
}
#endif
