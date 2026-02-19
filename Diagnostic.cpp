// Copyright 2018-2024 Johan Cockx
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
