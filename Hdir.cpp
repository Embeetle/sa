// Copyright 2018-2024 Johan Cockx
#include "Hdir.h"
#include "Project.h"
#include "File.h"
#include "Unit.h"
#include "base/filesystem.h"
#include "base/debug.h"

sa::Hdir::Hdir(Project *project, const std::string &path)
  : path(path), project(project)
{
  trace("create Hdir " << path);
  assert(!path.empty());
  assert(base::is_absolute_path(path));
}

sa::Hdir::~Hdir()
{
  trace("destroy Hdir " << path);
  project->erase_hdir(this);
}

void sa::Hdir::insert_occurrence(Occurrence *occurrence)
{
  trace("insert hdir occurrence " << *occurrence);
  assert(occurrence->get_hdir() == this);
  assert(_occurrences.find(occurrence) == _occurrences.end());
  _occurrences.insert(occurrence);
}

void sa::Hdir::remove_occurrence(Occurrence *occurrence)
{
  trace("remove hdir occurrence " << *occurrence);
  assert(occurrence->get_hdir() == this);
  assert(_occurrences.find(occurrence) != _occurrences.end());
  _occurrences.erase(occurrence);
}

void sa::Hdir::insert_linked_instance(Occurrence *occurrence)
{
  trace("insert hdir instance " << *occurrence);
  assert(project->is_locked());
  assert(occurrence->get_hdir() == this);
  if (!_inclusion_count) {
    trace_nest("Report hdir used: " << path);
    project->report_hdir_usage(this, InclusionStatus_included);
  }
  _inclusion_count++;
  assert(_inclusion_count);
}

void sa::Hdir::remove_linked_instance(Occurrence *occurrence)
{
  trace("remove hdir instance " << *occurrence);
  assert(project->is_locked());
  assert(occurrence->get_hdir() == this);
  assert(_inclusion_count);
  _inclusion_count--;
  if (!_inclusion_count) {
    trace_nest("Report hdir not used: " << path);
    project->report_hdir_usage(this, InclusionStatus_excluded);
  }
}

void sa::Hdir::update_flags_for_dependent_units(const char *reason)
{
  trace_nest("reload hdir " << path);
  assert(project->is_locked());
  // Hdir has been removed
  //
  // Determine the set of files that depend on this hdir. This will include
  // both C files and header files.
  std::set<File*> files;
  for (auto occurrence: _occurrences) {
    trace("add hdir occurrence " << *occurrence);
    occurrence->file->add_to_set_with_includers(files);
  }
  // Request a flags update for all files in the set. For header files, this
  // will remove analysis results if present. For other files, this will
  // schedule a background thread to extract up-to-date flags and reanalyze the
  // file if the flags have changed.
  for (auto file: files) {
    trace("Recompute flags for " << file->get_name() << " for hdir " << path);
    file->request_flags_update(reason);
  }
}

#ifndef NDEBUG
std::string sa::Hdir::get_debug_name() const
{
  return path;
}
#endif

