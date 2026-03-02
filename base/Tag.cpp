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

#include "Tag.h"
#include "debug.h"

using base::Taggable;
using base::TagBase;
using base::TagLink;
using base::Taggable;
using base::Taggable;
using base::Taggable;

Taggable::Taggable(): _taglink(0) {}
  
Taggable::~Taggable() {
  assert(is_valid(this));
  while (_taglink != 0) {
    _taglink->_base->remove_link(this);
  }
}

TagBase::TagBase()
{
  trace("TagBase " << this << " create");
}
    
TagBase::~TagBase()
{
  trace("TagBase " << this << " destroy");
}

void TagBase::remove_link(Taggable* taggable) {
  trace("TagBase " << this << " remove link for " << taggable);
  assert(is_valid(this));
  TagLink** linkp = &taggable->taglink();
  for (;;) {
    assert(*linkp != 0);
    if ((*linkp)->_base == this) break;
    linkp = &((*linkp)->_next);
  }
  TagLink* link = *linkp;
  *linkp = link->_next;
  destroy_link(link);
}

void TagBase::remove_link_of_last_object(Taggable* taggable) {
  trace("TagBase " << this << " remove link for last object " << taggable);
  assert(is_valid(this));
  TagLink** linkp = &taggable->taglink();
  for (;;) {
    assert(*linkp != 0);
    if ((*linkp)->_base == this) break;
    linkp = &((*linkp)->_next);
  }
  TagLink* link = *linkp;
  *linkp = link->_next;
  destroy_link_of_last_object(link);
}

void TagBase::exclude_link(Taggable* taggable) {
  trace("TagBase " << this << " exclude link for " << taggable);
  TagLink** linkp = &taggable->taglink();
  for (;;) {
    if (!*linkp) return;
    if ((*linkp)->_base == this) break;
    linkp = &((*linkp)->_next);
  }
  TagLink* link = *linkp;
  *linkp = link->_next;
  destroy_link(link);
}

#ifdef PROFILE
static unsigned total_calls_y = 0;
static unsigned total_calls_n = 0;
static unsigned total_travs_y = 0;
static unsigned total_travs_n = 0;
static unsigned max_travs_y = 0;
static unsigned max_travs_n = 0;

class OnExit {
public:
  ~OnExit() {
    cerr << "Y Tot=" << total_calls_y << " Av="
         << (float)total_travs_y/(float)total_calls_y
         << " max=" << max_travs_y << "\n";
    cerr << "N Tot=" << total_calls_n << " Av="
         << (float)total_travs_n/(float)total_calls_n
         << " max=" << max_travs_n << "\n";
    cerr << "  Av=" << (float)(total_travs_n+total_travs_y)
      /(float)(total_calls_n+total_calls_y)
         << " max=" << max_travs_n << "\n";
  }
};
OnExit onexit;
#endif

TagLink* TagBase::find_link(const Taggable* taggable) const {
  trace_nest("TagBase " << this << " find link for " << (void*)taggable);
  assert(is_valid(taggable));
#ifdef PROFILE
  unsigned travs = 1;
#endif
  TagLink* link = taggable->taglink();
  while (link != 0) {
    trace("find link " << (void*)link << " " << link->_base);
    assert(link->_base);
    if (link->_base == this) {
#ifdef PROFILE
      total_calls_y++;
      total_travs_y += travs;
      if (travs > max_travs_y) max_travs_y = travs;
#endif
      return link;
    }
    link = link->_next;
#ifdef PROFILE
    ++travs;
#endif
  }
#ifdef PROFILE
  total_calls_n++;
  total_travs_n += travs;
  if (travs > max_travs_n) max_travs_n = travs;
#endif
  return 0;
}

TagLink* TagBase::insert_link(Taggable* taggable) {
  trace("TagBase " << this << " insert link for " << taggable);
  TagLink* link = create_link(taggable);
  link->_base = this;
  link->_next = taggable->taglink();
  taggable->taglink() = link;
  return link;
}

TagLink* TagBase::include_link(Taggable* taggable) {
  trace_nest("include link");
  TagLink* link = find_link(taggable);
  if (!link) {
    link = insert_link(taggable);
  }
  return link;
}


