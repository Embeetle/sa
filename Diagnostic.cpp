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

#include "Diagnostic.h"
#include "Project.h"
#include "File.h"
#include "Unit.h"
#include <iostream>

sa::Diagnostic::Diagnostic(
  const std::string &message,
  Severity severity,
  Category category,
  File *file,
  const Location &location
)
  : message(message)
  , severity(severity)
  , category(category)
  , file(file)
  , location(location)
  , project(file->project)
{
  trace("create f diagnostic " << *this);
}

sa::Diagnostic::Diagnostic(
  const std::string &message,
  Severity severity,
  Category category,
  Project *project
)
  : message(message)
  , severity(severity)
  , category(category)
  , location()
  , project(project)
{
  trace("create p diagnostic " << *this);
}

sa::Diagnostic::Diagnostic()
  : Chain<Diagnostic>(this)
  , message("")
  , severity(Severity_none)
  , category(Category_none)
  , location()
  , project(0)
{
}

sa::Diagnostic::~Diagnostic()
{
  trace("destroy diagnostic " << *this);
  assert(!_instantiation_count);
  if (file) {
    assert(file->project->is_locked());
    file->drop_diagnostic(this);
  } else {
    project->release_diagnostic(this);
  }
}

void sa::Diagnostic::include_instance()
{
  assert(project->is_locked());
  trace("include diagnostic " << *this << " " << _instantiation_count);
  _instantiation_count++;
  assert(_instantiation_count);
}

void sa::Diagnostic::exclude_instance()
{
  assert(project->is_locked());
  trace("exclude diagnostic " << *this << " " << _instantiation_count);
  assert(_instantiation_count);
  _instantiation_count--;
}

void sa::Diagnostic::set_user_data(void *user_data)
{
  assert(project->is_locked());
  trace("  `-> set user data " << _user_data << " -> " << user_data);
  _user_data = user_data;
}

std::ostream &sa::operator<<(std::ostream &out, const Diagnostic &diagnostic)
{
  out << diagnostic.get_severity() << ": " << diagnostic.get_message();
  File *file = diagnostic.file;
  if (file) {
    out << " at " << file->get_name() << "." << diagnostic.location;
  }
  return out;
}

#ifndef NDEBUG
#include <sstream>

std::string sa::Diagnostic::get_debug_name() const
{
  std::stringstream buf;
  buf << *this;
  return buf.str();
}
#endif
