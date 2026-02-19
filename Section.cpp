// Copyright 2018-2024 Johan Cockx
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
