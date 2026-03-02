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

#include "source_analyzer.h"
#include "Project.h"
#include "Linker.h"
#include "File.h"
#include "GlobalSymbol.h"
#include "Symbol.h"
#include "Entity.h"
#include "Diagnostic.h"
#include "Unit.h"
#include "Task.h"
#include "base/ptr.h"
#include "base/filesystem.h"
#include "base/Timer.h"
#include <string>
#include <iostream>
#include <pthread.h>
#include <set>
#include "base/debug.h"
#include "base/os.h"

// TODO: use precompiled headers, see:
// https://stackoverflow.com/questions/20940111/
//    possible-to-share-work-when-parsing-multiple-files-with-libclang

// Assumption: application is single-threaded or uses a lock to sequentialize
// calls to the Clang engine. The Clang engine has locks to handle accesses by
// the application in parallel with multiple background threads, but not
// multiple application threads.
//
// As a consequence, no lock is needed when the application reads data that can
// only be changed by the application. To write such data, a lock is needed
// because the data can also be read by a background thread.

static std::set<sa::Project*> projects;

#define safe_access(index, array) (array[index])

extern "C" const char *sa::ce_project_status_name(ProjectStatus value)
{
  return safe_access(value, ProjectStatus_names);
}
      
extern "C" const char *sa::ce_linker_status_name(LinkerStatus value)
{
  return safe_access(value, LinkerStatus_names);
}
      
extern "C" const char *sa::ce_file_mode_name(FileMode value)
{
  return safe_access(value, FileMode_names);
}
     
extern "C" const char *sa::ce_file_kind_name(FileKind value)
{
  return safe_access(value, FileKind_names);
}
     
extern "C" const char *sa::ce_inclusion_status_name(InclusionStatus value)
{
  return safe_access(value, InclusionStatus_names);
}
      
extern "C" const char *sa::ce_analysis_status_name(AnalysisStatus value)
{
  return safe_access(value, AnalysisStatus_names);
}
      
extern "C" const char *sa::ce_entity_kind_name(EntityKind value)
{
  return safe_access(value, EntityKind_names);
}
  
extern "C" const char *sa::ce_occurrence_kind_name(OccurrenceKind value)
{
  return safe_access(value, OccurrenceKind_names);
}
  
extern "C" const char *sa::ce_link_status_name(LinkStatus value)
{
  return safe_access(value, LinkStatus_names);
}

extern "C" const char *sa::ce_severity_name(Severity severity)
{
  return safe_access(severity, Severity_names);
}

extern "C" const char *sa::ce_category_name(Category category)
{
  return safe_access(category, Category_names);
}

extern "C" sa::Project *sa::ce_create_project(
  const char *project_path,
  const char *cache_path,
  const char *resource_path,
  const char *lib_path,
  ProjectStatus_callback project_status_callback,
  InclusionStatus_callback inclusion_status_callback,
  LinkStatus_callback link_status_callback,
  AnalysisStatus_callback analysis_status_callback,
  AddSymbol_callback add_symbol_callback,
  DropSymbol_callback drop_symbol_callback,
  LinkerStatus_callback linker_status_callback,
  HdirUsage_callback hdir_usage_callback,
  AddDiagnostic_callback add_diagnostic_callback,
  RemoveDiagnostic_callback remove_diagnostic_callback,
  MoreDiagnostics_callback more_diagnostics_callback,
  AddOccurrenceInFile_callback add_occurrence_in_file_callback,
  RemoveOccurrenceInFile_callback remove_occurrence_in_file_callback,
  OccurrencesInFileCount_callback occurrences_in_file_count_callback,
  SetAlternativeContent_callback set_alternative_content_callback,
  AddOccurrenceOfEntity_callback add_occurrence_of_entity_callback,
  RemoveOccurrenceOfEntity_callback remove_occurrence_of_entity_callback,
  OccurrencesOfEntityCount_callback occurrences_of_entity_count_callback,
  SetOccurrenceOfEntityLinked_callback set_occurrence_of_entity_linked_callback,
  ReportInternalError_callback report_internal_error_callback,
  SetMemoryRegion_callback set_memory_region_callback,
  SetMemorySection_callback set_memory_section_callback,
  Utf8_callback utf8_callback,
  void *user_data
)
{
  std::cout << "SA compiled " __DATE__ " " __TIME__ "\n";
  assert_(base::is_absolute_path(project_path), project_path);
#ifndef NDEBUG
  if (!base::SA_DEBUG || *base::SA_DEBUG == '1') {
    std::string debug_out = std::string(project_path)+"/.beetle/sa_output.txt";
    if (base::SA_DEBUG) {
      std::cout << "SA debug output in " << debug_out << "\n";
    }
    debug_stream_pointer = new std::ofstream(debug_out);
    debug_writeln("SA debug output:");
  }
#endif
  trace_nest("ce_create_project " << project_path);
  Project *project = new Project(
    project_path,
    cache_path,
    resource_path,
    lib_path,
    project_status_callback,
    inclusion_status_callback,
    link_status_callback,
    analysis_status_callback,
    add_symbol_callback,
    drop_symbol_callback,
    linker_status_callback,
    hdir_usage_callback,
    add_diagnostic_callback,
    remove_diagnostic_callback,
    more_diagnostics_callback,
    add_occurrence_in_file_callback,
    remove_occurrence_in_file_callback,
    occurrences_in_file_count_callback,
    set_alternative_content_callback,
    add_occurrence_of_entity_callback,
    remove_occurrence_of_entity_callback,
    occurrences_of_entity_count_callback,
    set_occurrence_of_entity_linked_callback,
    report_internal_error_callback,
    set_memory_region_callback,
    set_memory_section_callback,
    utf8_callback,
    user_data
  );
  projects.insert(project);
  const char *SA_LIMIT = getenv("SA_LIMIT");
  int limit = SA_LIMIT ? atoi(SA_LIMIT) ? atoi(SA_LIMIT) : SIZE_MAX : 422;
  project->set_occurrences_in_file_limit(limit);
  project->set_occurrences_of_entity_limit(limit);
  return project;
}

extern "C" void sa::ce_set_toolchain_prefix(
  Project *project, const char *prefix
)
{
  assert(base::is_valid(project));
  trace_nest("ce_set_toolchain_prefix " << prefix);
  project->set_toolchain_prefix(prefix);
}

extern "C" void sa::ce_set_build_path(
  Project *project, const char *path
)
{
  assert(base::is_valid(project));
  trace_nest("ce_build_path " << path);
  Project::Lock lock(project);
  project->set_build_path(path);
}

extern "C" void sa::ce_drop_project(Project *project)
{
  assert(base::is_valid(project));
  trace_nest("ce_drop_project " << project);
  assert(!"not implemented yet");
  projects.erase(project);
}

extern "C" void sa::ce_add_hdir(
  Project *project,
  const char *path
)
{
  assert(base::is_valid(project));
  trace_nest("ce_add_hdir " << path);
  assert_(base::is_absolute_path(path), path);
  Project::Lock lock(project);
  project->add_hdir(path);
}

extern "C" void sa::ce_remove_hdir(Project *project, const char *path)
{
  assert(base::is_valid(project));
  trace_nest("ce_remove_hdir " << path);
  assert_(base::is_absolute_path(path), path);
  Project::Lock lock(project);
  project->remove_hdir(path);
}

extern "C" sa::File *sa::ce_get_file_handle(Project *project, const char *path)
{
  assert(base::is_valid(project));
  trace_nest("ce_get_file_handle " << path);
  Project::Lock lock(project);
  base::ptr<File> file = project->get_file(path);
  file->increment_known();
  return file;
}

extern "C" void sa::ce_drop_file_handle(File *file)
{
  assert(base::is_valid(file));
  trace_nest("ce_drop_file " << file->get_name());
  Project::Lock lock(file->project);
  assert(is_valid(file));
  trace("drop file " << file->get_name());
  file->decrement_known();
}

extern "C" sa::Project *sa::ce_get_file_project(File *file)
{
  assert(base::is_valid(file));
  trace_nest("ce_get_file_project " << file->get_name());
  return file->project;
}

extern "C" const char *sa::ce_get_file_path(File *file)
{
  assert(base::is_valid(file));
  trace_nest("ce_get_file_path " << file->get_name());
  return file->get_path().data();
}

extern "C" sa::FileMode sa::ce_get_file_mode(File *file)
{
  // File mode can only be changed by the application, so no need to lock the
  // project to read the mode from the application.
  assert(base::is_valid(file));
  trace_nest("ce_get_file_mode " << file->get_name());
  Project::Lock lock(file->project);
  return file->get_mode();
}

extern "C" sa::FileKind sa::ce_get_file_kind(File *file)
{
  // File kind is a constant,  computed at file construction time.
  assert(base::is_valid(file));
  trace_nest("ce_get_file_kind " << file->get_name());
  return file->file_kind;
}

extern "C" void *sa::ce_get_file_user_data(File *file)
{
  // File user data can only be changed by the application, so no need to lock
  // the project to read the user data from the application.
  assert(base::is_valid(file));
  trace_nest("ce_get_file_user_data " << file->get_name());
  return file->get_user_data();
}

extern "C" void sa::ce_add_file(File *file, FileMode mode, void *user_data)
{
  assert(base::is_valid(file));
  trace_nest(
    "ce_add_file " << file->get_name() << " " << mode << " " << user_data
    << " is-in-link-command: " << file->is_in_link_command()
  );
  Project::Lock lock(file->project);
  file->set_user_data(user_data);
  file->set_mode(mode);

  // Notify the project that this file exists. This may trigger a re-analysis if
  // there are missing headers. Note that the source analyser does not
  // spontanuously check for new files; it needs a trigger, and this is it.
  //
  // Notify the project even for non-headers: non-headers can also be included
  // in other compilation units.
  //
  // Notify the project even for files that already exist in the source
  // analyzer: it is possible that the file was removed temporarily on disk, but
  // that it still exists in the source analyzer because not all units including
  // it have been re-analysed, when it is added again.
  file->project->notify_file_exists(file->get_path());
  file->request_update("file was added");

  // The SA may have changed the file status before the file is added in the
  // client code. This can happen when the file is used in the link command or
  // included by an already added file.  When the client code later adds it, it
  // will assume - like for all new files - that the file is not used. We need
  // to fix that here.
  file->update_status_if_used();
}

extern "C" void sa::ce_set_file_mode(File *file, FileMode mode)
{
  assert(base::is_valid(file));
  trace_nest("ce_set_file_mode " << file->get_name() << " to " << mode);
  Project::Lock lock(file->project);
  file->set_mode(mode);
}

extern "C" void sa::ce_remove_file(File *file)
{
  assert(base::is_valid(file));
  trace_nest("ce_remove_file " << file->get_name());
  Project::Lock lock(file->project);
  file->set_user_data(0);
  file->set_mode(FileMode_exclude);

  // Reload all includers to generate the necessary missing header diagnostics
  file->reload("application calls remove_file()");
}

extern "C" void sa::ce_track_occurrences_in_file(
  File *file, unsigned occurrence_kinds, unsigned entity_kinds
)
{
  assert(base::is_valid(file));
  trace_nest("ce_track_occurrences_in_file "
    << OccurrenceKindSet(occurrence_kinds)
    << " " << EntityKindSet(entity_kinds) << " for " << file->get_name()
  );
  Project::Lock lock(file->project);
  file->track_occurrences_in_file(occurrence_kinds, entity_kinds);
}

extern "C" void sa::ce_track_occurrences_of_entity(
  Entity *entity, unsigned occurrence_kinds
)
{
  assert(base::is_valid(entity));
  trace_nest("ce_track_occurrences_of_entity "
    << OccurrenceKindSet(occurrence_kinds)
    << " of " << entity->get_entity_name()
  );
  Project::Lock lock(entity->project);
  entity->track_occurrences_of_entity(occurrence_kinds);
}

// To pass string lists from Python to C++, we encode them as zero-terminated
// strings in a single buffer, and pass them as buffer base pointer and size.
//
// Add items from a thus encoded string list to a C++ string_view list.
static void buffer_to_list(
  const char *buffer,
  unsigned size,
  std::vector<std::string_view> &list
)
{
  const char *end = buffer + size;
  const char *item = buffer;
  while (item < end) {
    size_t item_size = strlen(item);
    list.emplace_back(item, item_size);
    item += strlen(item) + 1;
  }
}

extern "C" void sa::ce_set_make_command(
  Project *project,
  const char *command_buffer,
  unsigned command_size
)
{
  assert(base::is_valid(project));
  trace_nest("ce_set_make_command");
  Project::Lock lock(project);
  std::vector<std::string_view> command;
  buffer_to_list(command_buffer, command_size, command);
  project->set_make_command(command);
}

#if 0
static std::vector<std::string> decode_string_buffer(
  const char *buffer, unsigned size
)
{
  std::vector<std::string> list;
  const char *item = buffer;
  const char *end = buffer + size;
  for (; item < end; item += strlen(item) + 1) {
    list.emplace_back(item);
  }
  return list;
}
#endif

extern "C" bool sa::ce_analysis_data_was_read_from_cache(File *file)
{
  assert(base::is_valid(file));
  trace_nest("ce_analysis_data_was_read_from_cache " << file->get_name());
  Project::Lock lock(file->project);
  return file->analysis_data_was_read_from_cache();
}

extern "C" void sa::ce_edit_file(
  File *file,
  unsigned begin,
  unsigned end,
  const char *new_content
)
{
  assert(base::is_valid(file));
  trace_nest("ce_edit_file " << begin << ".." << end << " '"
    << new_content << "' " << file->get_name()
  );
  Project::Lock lock(file->project);
  file->edit(Range(begin, end), new_content);
}

extern "C" void sa::ce_reload_file( File *file )
{
  assert(base::is_valid(file));
  trace_nest("ce_reload_file " << file->get_name());
  Project::Lock lock(file->project);
  file->reload("application calls reload_file()");
}

extern "C" sa::OccurrenceData sa::ce_find_occurrence(
  File *file,
  unsigned offset,
  unsigned begin_tol,
  unsigned end_tol
)
{
  assert(base::is_valid(file));
  trace_nest("ce_find_occurrence " << file->get_name() << "+" << offset);
  Project::Lock lock(file->project);
  return file->find_occurrence_data(offset, begin_tol, end_tol);
}

extern "C" void sa::ce_find_symbols(
  Project *project,
  const char *name,
  void (*find_symbol)(void *symbol_user_data, Entity *symbol, void *user_data),
  void *user_data
)
{
  assert(base::is_valid(project));
  trace_nest("ce_find_symbol " << name);
  {
    base::ptr<Symbol> symbol;
    {
      Project::Lock lock(project);
      symbol = project->find_global_symbol(name);
      if (symbol) {
        symbol->increment_known();
      }
    }
    if (symbol) {
      find_symbol(symbol->get_user_data(), symbol, user_data);
    }
  }
}

extern "C" unsigned sa::ce_get_completions(
  File *file,
  unsigned pos,
  void (*add_completion)(const char *completion, void *user_data),
  void *user_data,
  const char *context
)
{
  assert(base::is_valid(file));
  Project::Lock lock(file->project);
  trace_nest("ce_completions " << *file << "@" << pos
    << " context='" << context << "'"
  );
  return file->get_completions(pos, add_completion, user_data, context);
}

extern "C" sa::RangeData sa::ce_find_empty_loop(File *file, unsigned offset)
{
  assert(base::is_valid(file));
  trace_nest("ce_find_empty_loop " << *file << "@" << offset);
  Project::Lock lock(file->project);
  Range range = file->find_empty_loop(offset);
  RangeData data = { range.begin, range.end };
  return data;
}

extern "C" void sa::ce_drop_entity_handle(Entity *entity)
{
  assert(base::is_valid(entity));
  trace_nest("ce_drop_entity_handle " << entity);
  Project::Lock lock(entity->project);
  assert(entity->is_known());
  entity->decrement_known();
}

extern "C" void sa::ce_set_diagnostic_limit(
  Project *project, Severity severity, unsigned limit
)
{
  assert(base::is_valid(project));
  Project::Lock lock(project);
  trace_nest("set diagnostic limit " << severity << "=" << limit);
  project->set_diagnostic_limit(severity, limit);
}

extern "C" void sa::ce_set_number_of_workers(unsigned n)
{
  trace_nest("ce_set_number_of_workers " << n);
  Task::set_number_of_workers(n);
}

extern "C" void sa::ce_start()
{
  trace_nest("ce_start");
  Task::start();
}

extern "C" void sa::ce_stop()
{
  trace_nest("ce_stop");
  Task::stop();
}

extern "C" void sa::ce_abort()
{
  trace_nest("ce_abort");
#ifdef CHECK
  base::RefCounted::report_all();
#endif
  Task::abort();
}

extern "C" void sa::ce_check(base::Checked *pointer)
{
  trace_nest("ce_check");
  assert(base::is_valid(pointer));
}

#if 0
// Code for quick experiments with callbacks
typedef void* (*CreateCallback)();
typedef void (*DestroyCallback)(void* arg);

// dss dsddf
//

extern "C" void test(CreateCallback create, DestroyCallback destroy)
{
#if 0
  trace_nest("test");
  void *foo = create();
  trace("foo: " << foo);
  destroy(foo);
#else
  (void)create;
  (void)destroy;
#endif
}
#endif

