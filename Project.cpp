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

#include "Project.h"
#include "File.h"
#include "Hdir.h"
#include "Symbol.h"
#include "GlobalSymbol.h"
#include "LocalSymbol.h"
#include "Diagnostic.h"
#include "Linker.h"
#include "Unit.h"
#include "FlagExtractor.h"
#include "Unit.h"
#include "base/Vector.h"
#include "base/filesystem.h"
#include "base/os.h"
#include "base/Timer.h"
#include "base/debug.h"
#include <iostream>
#include <utility>

sa::Project::Project(
  const std::string &project_path,
  const std::string &cache_path,
  const std::string &resource_path,
  const std::string &lib_path,
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
: linker(new Linker(this))
, flag_extractor(new FlagExtractor(linker))
, project_path(base::get_normalized_path(project_path))
, cache_path(base::get_normalized_path(
    base::get_absolute_path(cache_path, project_path)
  ))
, resource_path(base::get_normalized_path(resource_path))
, lib_path(base::get_normalized_path(lib_path))
, user_data(user_data)
, project_status_callback(project_status_callback)
, inclusion_status_callback(inclusion_status_callback)
, link_status_callback(link_status_callback)
, analysis_status_callback(analysis_status_callback)
, add_symbol_callback(add_symbol_callback)
, drop_symbol_callback(drop_symbol_callback)
, linker_status_callback(linker_status_callback)
, hdir_usage_callback(hdir_usage_callback)
, add_diagnostic_callback(add_diagnostic_callback)
, remove_diagnostic_callback(remove_diagnostic_callback)
, more_diagnostics_callback(more_diagnostics_callback)
, add_occurrence_in_file_callback(add_occurrence_in_file_callback)
, remove_occurrence_in_file_callback(remove_occurrence_in_file_callback)
, occurrences_in_file_count_callback(occurrences_in_file_count_callback)
, set_alternative_content_callback(set_alternative_content_callback)
, add_occurrence_of_entity_callback(add_occurrence_of_entity_callback)
, remove_occurrence_of_entity_callback(remove_occurrence_of_entity_callback)
, occurrences_of_entity_count_callback(occurrences_of_entity_count_callback)
, set_occurrence_of_entity_linked_callback(
  set_occurrence_of_entity_linked_callback
)
, report_internal_error_callback(report_internal_error_callback)
, set_memory_region_callback(set_memory_region_callback)
, set_memory_section_callback(set_memory_section_callback)
, utf8_callback(utf8_callback)
{
  trace_nest("create project " << project_path);
  trace("resource path: " << resource_path);
  assert(inclusion_status_callback);
  assert(link_status_callback);
  assert(hdir_usage_callback);
  assert(analysis_status_callback);
  assert(project_status_callback);
  assert(linker_status_callback);
  assert(add_symbol_callback);
  assert(drop_symbol_callback);
  assert(add_diagnostic_callback);
  assert(remove_diagnostic_callback);
  assert(more_diagnostics_callback);
  assert(add_occurrence_in_file_callback);
  assert(set_alternative_content_callback);
  assert(add_occurrence_of_entity_callback);
  assert(set_memory_region_callback);
  assert(set_memory_section_callback);
  assert(utf8_callback);
  for (unsigned i = 0; i < NumberOfSeverities; ++i) {
    first_hidden_diagnostic[i] = diagnostic_chain + i;
  }
  set_toolchain_prefix("");
  Lock lock(this);
  base::create_directory(this->cache_path); // ignore error
}

sa::Project::~Project()
{
  trace_nest("destroy Project");
  assert(_global_symbols.empty());
  delete flag_extractor;
  delete linker;
}

void sa::Project::set_toolchain_prefix(const std::string &prefix)
{
  Lock lock(this);
  toolchain_prefix = prefix;
}

void sa::Project::set_build_path(const std::string &path)
{
  assert(is_locked());
  trace_nest("Project set build path to: " << path);
  std::string new_path = base::get_normalized_path(
    base::get_absolute_path(path, project_path)
  );
  if (build_path != new_path) {
    build_path = new_path;
    flag_extractor->set_build_directory(build_path);
  }
}

std::string sa::Project::get_natural_path(const std::string &path)
{
  return base::get_natural_path(
    base::get_normalized_path(path), get_project_path()
  );
}

std::string sa::Project::get_build_path(const std::string &path)
{
  std::string abs_path = 
    base::get_absolute_path(base::get_normalized_path(path), get_project_path());
  if (base::is_nested_in(abs_path, get_project_path())) {
    return base::get_relative_path(abs_path, build_path);
  } else {
    return abs_path;
  }
}

std::string sa::Project::get_unique_path(const std::string &path)
{
  std::string result = get_natural_path(path);
  base::os::normalize_path_case(result);
  return result;
}

const std::string &sa::Project::get_build_path() const
{
  assert(is_locked());
  return build_path;
}

void sa::Project::set_make_command(
  std::vector<std::string_view> command
)
{
  assert(is_locked());
  flag_extractor->set_make_command(command);
}

void sa::Project::analyze_make_command()
{
  assert(is_locked());
  trace_nest("Project analyze make command");
  // Temp hack: updating the build directory forces the make command analyzer to
  // re-analyze everything. Once Python flag extraction is gone, we probably
  // don't need this method anymore.
  flag_extractor->set_build_directory(build_path);
}

void sa::Project::fail(const char *message)
{
  debug_writeln("internal error '" << message << "' in " << project_path);
  if (report_internal_error_callback) {
    report_internal_error_callback(message, user_data);
  } else {
    debug_writeln("no internal error callback configured for this project");
  }
}

const std::string &sa::Project::get_cache_path() const
{
  return cache_path;
}

const std::string sa::Project::get_clang_resource_path() const
{
  return resource_path;
}

const std::string sa::Project::get_asm_analyzer_path() const
{
  return lib_path + "/clang" + base::os::get_exe_extension();
}

const std::string sa::Project::get_linker_script_analyzer_path() const
{
  return lib_path + "/lld" + base::os::get_exe_extension();
}

const std::string &sa::Project::get_toolchain_prefix() const
{
  Lock lock(this);
  return toolchain_prefix;
}

base::ptr<sa::File> sa::Project::get_file(const std::string &raw_path)
{
  assert(is_locked());
  trace_nest("FileMap::get_file " << raw_path);
  std::string path = base::get_normalized_path(
    base::get_absolute_path(raw_path, get_project_path())
  );
  std::string key = base::normalize_path_case(path);
  auto it = _file_map.find(key);
  base::ptr<File> file;
  if (it == _file_map.end()) {
    base::patch_path_to_case_on_disk(path);
    trace_nest("insert file: " << path);
    file = base::ptr<File>::create(this, path);
    _file_map.insert(std::make_pair(key, (File*)file));
  } else {
    file = it->second;
    assert(base::is_valid(file));
    trace_nest("existing file " << file->get_path());
  }
  assert(is_valid((File*)file));
  return file;
}

void sa::Project::erase_file(File *file)
{
  assert(base::is_valid(file));
  assert(is_locked());
  std::string key = base::normalize_path_case(file->get_path());
  _file_map.erase(key);
}

void sa::Project::report_file_inclusion_status(
  File const *file, unsigned status
)
{
  trace_nest("report file inclusion status " << status
    << " for " << file->get_path()
  );
  assert(base::is_valid(file));
  assert(file->is_known());
  debug_lock_output;
  inclusion_status_callback(
    file->get_path().data(),
    file->get_user_data(),
    status
  );
}

void sa::Project::report_file_link_status(
  File const *file, unsigned status
)
{
  trace_nest("report file link status " << status
    << " for " << file->get_path()
  );
  assert(base::is_valid(file));
  assert(file->is_known());
  debug_lock_output;
  link_status_callback(
    file->get_path().data(),
    file->get_user_data(),
    status
  );
}

base::ptr<sa::Hdir> sa::Project::get_hdir(const std::string &raw_path)
{
  assert(is_locked());
  trace_nest("get_hdir " << raw_path);
  std::string path = base::get_normalized_path(
    base::get_absolute_path(raw_path, get_project_path())
  );
  std::string key = base::normalize_path_case(path);
  trace("key is " << path);
  base::ptr<sa::Hdir> hdir;
  auto it = _hdir_map.find(key);
  if (it == _hdir_map.end()) {
    hdir = base::ptr<sa::Hdir>::create(this, path);
    assert(hdir);
    _hdir_map[key] = hdir;
    trace("created hdir " << hdir->path);
  } else {
    hdir = it->second;
    assert(base::is_valid(hdir));
    trace("reused hdir " << hdir->path);
  }
  return hdir;
}

void sa::Project::erase_hdir(const Hdir *hdir)
{
  assert(base::is_valid(hdir));
  assert(is_locked());
  std::string key = base::normalize_path_case(hdir->path);
  _hdir_map.erase(key);
}

void sa::Project::report_hdir_usage(const Hdir *hdir, InclusionStatus status)
{
  assert(base::is_valid(hdir));
  hdir_usage_callback(hdir->path.data(), status);
}

inline bool is_error_status(sa::AnalysisStatus status)
{
  return status == sa::AnalysisStatus_failed;
}

inline bool is_pending_status(sa::AnalysisStatus status)
{
  return status == sa::AnalysisStatus_waiting
    || status == sa::AnalysisStatus_busy;
}

void sa::Project::report_file_analysis_status(
  File const *file, 
  AnalysisStatus old_status,
  AnalysisStatus new_status
)
{
  assert(base::is_valid(file));
  trace_nest("Project::report_file_analysis_status "
    << old_status << " -> " << new_status << " " << file->get_name()
  );
  debug_lock_output;
  analysis_status_callback(
    file->get_path().data(),
    file->get_user_data(),
    old_status, new_status
  );
}

void sa::Project::update_for_analysis_status_change(
  Unit *unit,
  AnalysisStatus old_status,
  AnalysisStatus new_status,
  const char *reason
)
{
  File *file = unit->file;
  assert(base::is_valid(file));
  assert(is_locked());
  trace_nest("project update analysis status for " << file->get_name()
    << " (is_link_candidate=" << file->is_link_candidate()
    << ") from " << old_status << " to " << new_status << " reason: " << reason
  );
  assert(old_status != new_status);
  assert(file->is_known());
  report_file_analysis_status(file, old_status, new_status);
  trace("old pending: " << is_pending_status(old_status));
  trace("new pending: " << is_pending_status(new_status));
  if (file->is_link_candidate()) {
    // Increment pending count first and decrement it last to avoid spurious
    // linker status changes.
    if (is_pending_status(new_status)) {
      if (!is_pending_status(old_status)) {
        inc_pending_file_count();
      }
    } else {
      if (is_pending_status(old_status)) {
        dec_pending_file_count();
      }
    }
    // Increment failed count first and decrement it last to avoid spurious
    // project status changes.
    if (new_status == AnalysisStatus_failed) {
      inc_failed_file_count();
    }
    if (old_status == AnalysisStatus_failed) {
      dec_failed_file_count();
    }
  }
}

void sa::Project::update_link_candidate_status(
  File *file,
  bool new_is_link_candidate,
  AnalysisStatus status
)
{
  assert(is_locked());
  trace_nest("project: update link candidate status to "
    << new_is_link_candidate << " " << status
    << " " << file->get_name()
  );
  if (new_is_link_candidate) {
    if (is_pending_status(status)) {
      trace("inc pending file count");
      inc_pending_file_count();
    }
    if (status == AnalysisStatus_failed) {
      inc_failed_file_count();
    }
  } else {
    if (is_pending_status(status)) {
      trace("dec pending file count");
      dec_pending_file_count();
    }
    if (status == AnalysisStatus_failed) {
      dec_failed_file_count();
    }
  }
}

void sa::Project::inc_pending_file_count()
{
  assert(is_locked());
  trace_nest("inc pending file count to " << (_nr_pending_files+1));
  if (!_nr_pending_files) {
    set_project_status(ProjectStatus_busy);
  }
  _nr_pending_files++;
  assert(_nr_pending_files);
}

void sa::Project::dec_pending_file_count()
{
  assert(is_locked());
  assert(_nr_pending_files);
  _nr_pending_files--;
  trace_nest("dec pending file count to " << _nr_pending_files
    << " failed=" << _nr_failed_files
  );
  if (!_nr_pending_files) {
    if (_nr_failed_files) {
      set_project_status(ProjectStatus_failed);
    } else {
      linker->trigger();
    }
    base::Timer::report_all();
  }
}

void sa::Project::inc_failed_file_count()
{
  trace_nest("inc failed file count to " << (_nr_failed_files+1));
  if (!_nr_failed_files) {
    if (project_status == ProjectStatus_ready) {
      set_project_status(ProjectStatus_failed);
    }
  }
  _nr_failed_files++;
  assert(_nr_failed_files);
}

void sa::Project::dec_failed_file_count()
{
  assert(_nr_failed_files);
  _nr_failed_files--;
  trace_nest("dec failed file count to " << _nr_failed_files);
  if (!_nr_failed_files) {
    if (project_status == ProjectStatus_failed) {
      set_project_status(ProjectStatus_ready);
    }
  }
}

void sa::Project::set_project_status(ProjectStatus new_project_status)
{
  assert(is_locked());
  trace_nest("Set project status " << project_status << " -> "
    << new_project_status
  );
  if (project_status != new_project_status) {
    project_status = new_project_status;
    debug_lock_output;
    project_status_callback(this, project_status);
  }
}

void sa::Project::set_linker_results(
  const link_status_map_type &link_status_map,
  base::ptr<GlobalSymbol> main
)
{
  trace_nest("set linker results");
  assert(base::is_valid(main));
  assert(is_locked());
  //
  // Update link status of global symbols. The names of global symbols with link
  // status different from none are stored in the _global_names set, to avoid
  // iterating over all global symbols.
  //
  // TODO: now that there is only one global symbol with a given name, keep a
  // list of symbols instead of a list of names.
  //
  // Remove names from the set of global names if they are no longer in the
  // linker's link status map.
  //
  // Careful: the loop below will erase global symbols while iterating over the
  // set.  Simply erasing an element using an iterator invalidates the iterator.
  for (auto it = _global_names.begin(); it != _global_names.end(); ) {
    auto name = *it;
    if (link_status_map.find(name.data()) == link_status_map.end()) {
      trace_nest("Unlink " << name);
      it = _global_names.erase(it);
      auto symbol = find_global_symbol(name.data());
      if (symbol) {
        assert(base::is_valid(symbol));
        symbol->set_link_status(LinkStatus_none);
      }
    } else {
      ++it;
    }
  }
  // Add names to the set of global names if they are in the linker's link
  // status map.
  for (auto const &[name, status]: link_status_map) {
    trace_nest("Link " << name << ": " << status);
    assert(status != LinkStatus_none);
    auto symbol = find_global_symbol(name);
    if (symbol) {
      assert(base::is_valid(symbol));
      symbol->set_link_status(status);
    }
    _global_names.insert(name);
  }
  //
  // Handle link diagnostics for link status of main function.
  LinkStatus new_main_link_status =
    link_status_map.find(main->link_name.data())->second;
  if (main_link_status != new_main_link_status) {
    if (main_link_diagnostic) {
      remove_diagnostic(main_link_diagnostic);
      main_link_diagnostic = 0;
    }
    main_link_status = new_main_link_status;
    if (main_link_status == LinkStatus_undefined) {
      main_link_diagnostic = get_diagnostic(
        "Undefined function 'main'", Severity_error
      );
      add_diagnostic_at_back(main_link_diagnostic);
    }
  }
  //
  for (unsigned i = NumberOfSeverities; i--; ) {
    report_hidden_diagnostics((Severity)i);
  }
  set_project_status(ProjectStatus_ready);
}

void sa::Project::report_linker_status(LinkerStatus status)
{
  debug_lock_output;
  linker_status_callback(this, status);
}

void sa::Project::set_alternative_content(
  File const *file,
  const char *content
)
{
  set_alternative_content_callback(
    file->get_path().data(),
    file->get_user_data(),
    content
  );
}

void sa::Project::add_symbol(Symbol *symbol)
{
  assert(base::is_valid(symbol));
  assert(is_locked());
  trace_nest("add symbol callback");
  assert(symbol->is_known());
  symbol->set_user_data(
    add_symbol_callback(
      symbol->get_name_data(),
      symbol->get_effective_kind(),
      symbol
    )
  );
}

void sa::Project::drop_symbol(Symbol *symbol)
{
  assert(base::is_valid(symbol));
  trace_nest("drop symbol callback");
  drop_symbol_callback(symbol->get_user_data());
}

void sa::Project::erase_global_symbol(GlobalSymbol *symbol)
{
  assert(base::is_valid(symbol));
  assert(is_locked());
  trace("Project erase_global_symbol " << symbol << " " << *symbol);
  _global_symbols.erase(symbol->link_name);
}

base::ptr<sa::GlobalSymbol> sa::Project::get_global_symbol(
  const std::string &link_name
)
{
  trace_nest("Project::get_global_symbol " << link_name);
    
  // Make sure _global_symbols does not change while we have a reference to one
  // of its items.
  Lock lock(this);
  return _get_global_symbol(link_name);
}
  
base::ptr<sa::GlobalSymbol> sa::Project::_get_global_symbol(
  const std::string &link_name
)
{
  trace_nest("Project::_get_global_symbol " << link_name);
  assert(is_locked());
  GlobalSymbol *&symbol = _global_symbols[link_name];
  if (symbol) {
    trace("got " << symbol << " " << *symbol);
    return symbol;
  }
  auto symbol_ptr = base::ptr<GlobalSymbol>::create(link_name, this);
  trace("created " << symbol_ptr << " " << *symbol_ptr);
  symbol = symbol_ptr;
  return symbol_ptr;
}

base::ptr<sa::GlobalSymbol> sa::Project::find_global_symbol(
  const std::string &link_name
) const
{
  assert(is_locked());
  trace_nest("Project find_global_symbol " << link_name);
  auto it = _global_symbols.find(link_name);
  if (it == _global_symbols.end()) {
    return 0;
  }
  return it->second;
}

void sa::Project::erase_local_symbol(LocalSymbol *symbol)
{
  assert(base::is_valid(symbol));
  trace("Project erase local symbol " << symbol << " " << *symbol);
  assert(is_locked());
  symbol->get_ref_location().file->erase_local_symbol(symbol);
}

base::ptr<sa::LocalSymbol> sa::Project::get_local_symbol(
  EntityKind kind,
  const std::string &user_name,
  const FileLocation &ref_location,
  Occurrence *ref_scope
)
{
  return ref_location.file->get_local_symbol(
    kind, user_name, ref_location.offset, ref_scope
  );
}

void sa::Project::add_unit_diagnostics(
  std::vector<base::ptr<Diagnostic>> &diagnostics
)
{
  trace_nest("add unit diagnostics");
  assert(is_locked());
  Diagnostic *pos[NumberOfSeverities];
  for (unsigned i = 0; i < NumberOfSeverities; ++i) {
    pos[i] = diagnostic_chain + i;
  }
  for (auto diagnostic: diagnostics) {
    assert(base::is_valid(diagnostic));
    trace("include diagnostic " << *diagnostic);
    Diagnostic *&after = pos[diagnostic->get_severity()];
    if (!diagnostic->is_instantiated()) {
      trace("add it");
      assert_(!diagnostic->is_visible, *diagnostic);
      add_diagnostic_after(diagnostic, after);
    }
    diagnostic->include_instance();
    after = diagnostic;
  }
}

void sa::Project::remove_unit_diagnostics(
  std::vector<base::ptr<Diagnostic>> &diagnostics
)
{
  trace_nest("remove unit diagnostics");
  assert(is_locked());
  for (auto diagnostic: diagnostics) {
    assert(base::is_valid(diagnostic));
    trace("exclude diagnostic " << *diagnostic);
    diagnostic->exclude_instance();
    if (!diagnostic->is_instantiated()) {
      remove_diagnostic(diagnostic);
    }
  }
}

void sa::Project::reload_as_config_file(File *file)
{
  assert(is_locked());
  assert(base::is_valid(file));
  linker->reload_as_linker_script(file);
  flag_extractor->reload_as_makefile(file);
}

void sa::Project::set_memory_region(
  const std::string &name,
  bool present,
  size_t origin,
  size_t size
)
{
  set_memory_region_callback(name.data(), present, origin, size);
}

void sa::Project::set_memory_section(
  const std::string &name,
  bool present,
  const std::string &runtime_region,
  const std::string &load_region
)
{
  set_memory_section_callback(
    name.data(), present, runtime_region.data(), load_region.data()
  );
}

void sa::Project::add_link_diagnostic(
  Occurrence *occurrence,
  Diagnostic *diagnostic
)
{
  assert(is_valid(occurrence));
  assert(is_valid(diagnostic));
  trace_nest("add link diagnostic at " << *occurrence << ": " << *diagnostic);
  assert(is_locked());
  assert(diagnostics.find(occurrence) == diagnostics.end());
  diagnostics[occurrence] = diagnostic;
  if (!diagnostic->is_instantiated()) {
    add_diagnostic_at_back(diagnostic);
  }
  diagnostic->include_instance();
}
    
void sa::Project::remove_link_diagnostic(Occurrence *occurrence)
{
  assert(is_valid(occurrence));
  trace_nest("remove link diagnostic at " << *occurrence);
  assert(is_locked());
  auto it = diagnostics.find(occurrence);
  assert_(it != diagnostics.end(), *occurrence);
  auto diagnostic = it->second;
  diagnostics.erase(it);
  diagnostic->exclude_instance();
  if (!diagnostic->is_instantiated()) {
    remove_diagnostic(diagnostic);
  }
}

void sa::Project::add_diagnostic_at_front(Diagnostic *diagnostic)
{
  assert(is_valid(diagnostic));
  assert_(!diagnostic->is_visible, *diagnostic);
  add_diagnostic_after(
    diagnostic, diagnostic_chain + diagnostic->get_severity()
  );
}

void sa::Project::add_diagnostic_at_back(Diagnostic *diagnostic)
{
  assert(is_valid(diagnostic));
  trace("add diagnostic at back: " << *diagnostic);
  assert_(!diagnostic->is_visible, *diagnostic);
  add_diagnostic_before(
    diagnostic, diagnostic_chain + diagnostic->get_severity()
  );
}

void sa::Project::add_diagnostic_before(
  Diagnostic *diagnostic, Diagnostic *before
)
{
  add_diagnostic_after(diagnostic, before->prev);
}

void sa::Project::add_diagnostic_after(Diagnostic *diagnostic, Diagnostic *after)
{
  assert(is_valid(diagnostic));
  assert(is_valid(after));
  assert(is_locked());
  trace_nest("add diagnostic " << *diagnostic << " after " << *after);
  assert(is_valid(diagnostic));
  assert(!diagnostic->get_user_data());
  assert_(!diagnostic->is_visible, *diagnostic);
  diagnostic->insert_after(after);
  //
  // Show the new diagnostic if possible
  Severity severity = diagnostic->get_severity();
  Diagnostic *&first_hidden = first_hidden_diagnostic[severity];
  size_t &budget = diagnostic_budget[severity];
  trace("budget=" << budget);
  if (budget) {
    // There is still budget, so there should be no hidden diagnostics yet
    assert(first_hidden == diagnostic_chain + severity);
    budget--;
    show_diagnostic(diagnostic);
  } else if (diagnostic->next->is_visible) {
    // There is no budget, and we have inserted before a visible diagnostic.
    // Hide the last visible one before showing the new one.
    first_hidden = first_hidden->prev;
    hide_diagnostic(first_hidden);
    show_diagnostic(diagnostic);
  } else if (diagnostic->next == first_hidden) {
    // There is no budget, and we have inserted just before the first hidden
    // diagnostic.  The new diagnostic now becomes the first hidden one.
    first_hidden = first_hidden->prev;
  }
}

void sa::Project::remove_diagnostic(Diagnostic *diagnostic)
{
  assert(is_valid(diagnostic));
  assert(is_locked());
  trace_nest("remove diagnostic " << *diagnostic);
  Severity severity = diagnostic->get_severity();
  Diagnostic *&first_hidden = first_hidden_diagnostic[severity];
  size_t &budget = diagnostic_budget[severity];
  if (diagnostic->is_visible) {
    hide_diagnostic(diagnostic);
    if (first_hidden == diagnostic_chain + severity) {
      // There are no hidden diagnostics to replace the one just hid.
      budget++;
    } else {
      // There is a hidden diagnostic. Since we removed a visible one, we can
      // show a previously hidden one. Note that the first hidden diagnostic
      // canot be the removed diagnostic, because the removed diagnostic is
      // visible.
      show_diagnostic(first_hidden);
      first_hidden = first_hidden->next;
    }
  } else if (diagnostic == first_hidden) {
    first_hidden = first_hidden->next;
  }
  diagnostic->remove();
}

void sa::Project::show_diagnostic(Diagnostic *diagnostic)
{
  assert(is_valid(diagnostic));
  trace_nest("show diagnostic " << *diagnostic);
  assert(diagnostic != diagnostic_chain + diagnostic->get_severity());
  assert(!diagnostic->is_visible);
  diagnostic->is_visible = true;
  File *file = diagnostic->file;
  assert(!file || file->is_known());
  assert(!diagnostic->get_user_data());
  diagnostic->set_user_data(
    add_diagnostic_callback(
      diagnostic->get_message().data(),
      diagnostic->get_severity(),
      diagnostic->get_category(),
      file ? file->get_path().data() : 0,
      file ? file->get_user_data() : 0,
      diagnostic->location.offset,
      diagnostic->prev->get_user_data()
    )
  );
}

void sa::Project::hide_diagnostic(Diagnostic *diagnostic)
{
  assert(is_valid(diagnostic));
  trace_nest("hide diagnostic " << *diagnostic);
  assert(diagnostic != diagnostic_chain + diagnostic->get_severity());
  assert(diagnostic->is_visible);
  diagnostic->is_visible = false;
  void *user_data = diagnostic->get_user_data();
  if (user_data) {
    remove_diagnostic_callback(user_data);
    diagnostic->set_user_data(0);
  }
}

size_t sa::Project::get_diagnostic_limit(Severity severity)
{
  assert(is_locked());
  return diagnostic_limit[severity];
}

void sa::Project::set_diagnostic_limit(Severity severity, size_t new_limit)
{
  trace_nest("set diagnostic limit for " << severity << " to " << new_limit);
  assert(is_locked());
  size_t &limit = diagnostic_limit[severity];
  if (limit != new_limit) {
  
    size_t &budget = diagnostic_budget[severity];
    size_t &hidden_count = hidden_diagnostic_count[severity];
    bool &hidden_count_changed = hidden_diagnostic_count_changed[severity];
    Diagnostic *&first_hidden = first_hidden_diagnostic[severity];
    trace("old limit: " << limit << ", budget: " << budget
      << ", hidden: " << hidden_count
    );

    if (new_limit < limit) {
      if (budget < limit - new_limit) {
        while (budget < limit - new_limit) {
          first_hidden = first_hidden->prev;
          hide_diagnostic(first_hidden);
          hidden_count++;
          budget++;
        }
        hidden_count_changed = true;
        report_hidden_diagnostics(severity);
      }
      budget -= limit - new_limit;
    } else {
      budget += new_limit - limit;
      if (hidden_count) {
        while (hidden_count && budget) {
          budget--;
          hidden_count--;
          show_diagnostic(first_hidden);
          first_hidden = first_hidden->next;
        }
        hidden_count_changed = true;
        report_hidden_diagnostics(severity);
      }
    }
    limit = new_limit;
  }
}

void sa::Project::report_hidden_diagnostics(Severity severity)
{
  if (hidden_diagnostic_count_changed[severity]) {
    hidden_diagnostic_count_changed[severity] = false;
    more_diagnostics_callback(this, severity, hidden_diagnostic_count[severity]);
  }
}

void sa::Project::add_occurrence_in_file(Occurrence *occurrence)
{
  assert(is_valid(occurrence));
  assert(is_locked());
  trace_nest("add occ in file " << (void*)occurrence << " " << *occurrence);
  grab_tracking_in_file_user_data(occurrence);
}

void sa::Project::remove_occurrence_in_file(Occurrence *occurrence)
{
  assert(is_valid(occurrence));
  assert(is_locked());
  trace_nest("remove occ in file " << occurrence);
  drop_tracking_in_file_user_data(occurrence);
}

size_t sa::Project::get_occurrences_in_file_limit()
{
  return _reported_in_file_limit;
}

void sa::Project::set_occurrences_in_file_limit(size_t new_limit)
{
  _reported_in_file_limit = new_limit;
  check_occurrences_in_file_limit();
}
  
void sa::Project::check_occurrences_in_file_limit()
{
  while (_reported_in_file_map.size() > _reported_in_file_limit) {
    auto it = _reported_in_file_map.begin();
    Occurrence *occurrence = it->first;
    void *user_data = it->second.first;
    size_t count = it->second.second;
    withdraw_occurrence_in_file(occurrence, user_data);
    _unreported_in_file_map.insert(std::make_pair(occurrence, count));
    _reported_in_file_map.erase(it);
  }
  while (_reported_in_file_map.size() < _reported_in_file_limit
    && !_unreported_in_file_map.empty()
  ) {
    auto it = _unreported_in_file_map.begin();
    Occurrence *occurrence = it->first;
    size_t count = it->second;
    void *user_data = report_occurrence_in_file(occurrence);
    _reported_in_file_map.insert(
      std::make_pair(occurrence, std::make_pair(user_data,count))
    );
    _unreported_in_file_map.erase(it);
  }
}

void sa::Project::update_occurrence_in_file(Occurrence *occurrence)
{
  assert(is_valid(occurrence));
  assert(is_locked());
  trace_nest("update occ in file " << occurrence);
  auto it = _reported_in_file_map.find(occurrence);
  if (it != _reported_in_file_map.end()) {
    size_t count = it->second.second;
    for (size_t i = count; i--; ) {
      drop_tracking_in_file_user_data(occurrence);
    }
    for (size_t i = count; i--; ) {
      grab_tracking_in_file_user_data(occurrence);
    }
  }
}

void *sa::Project::report_occurrence_in_file(Occurrence *occurrence)
{
  trace_nest("report occurrence in file: " << *occurrence);
  Entity *entity = occurrence->entity;
  Occurrence *scope = entity->get_ref_scope();
  void *scope_user_data = scope ? grab_tracking_in_file_user_data(scope) : 0;
  entity->increment_known();
  occurrence->file->increment_known();
  void *user_data = add_occurrence_in_file_callback(
    occurrence->file->get_path().data(),
    occurrence->get_range().begin,
    occurrence->get_range().end,
    occurrence->kind,
    entity->get_user_data(),
    entity,
    scope_user_data,
    occurrence->is_linked()
  );
  return user_data;
}

void sa::Project::withdraw_occurrence_in_file(
  Occurrence *occurrence, void *user_data
)
{
  trace_nest("withdraw occurrence in file: " << *occurrence);
  if (remove_occurrence_in_file_callback) {
    remove_occurrence_in_file_callback(user_data);
  }
  Entity *entity = occurrence->entity;
  Occurrence *scope = entity->get_ref_scope();
  assert(base::is_valid(occurrence->file));
  assert(base::is_valid(entity));
  assert(base::is_valid_or_null(scope));
  occurrence->file->decrement_known();
  entity->decrement_known();
  if (scope) {
    drop_tracking_in_file_user_data(scope);
  }
}

void* sa::Project::grab_tracking_in_file_user_data(Occurrence *occurrence)
{
  assert(base::is_valid(occurrence));
  trace_nest("grab occ in file " << (void*)occurrence << " " << *occurrence);
  assert(is_locked());
  auto it = _unreported_in_file_map.find(occurrence);
  if (it != _unreported_in_file_map.end()) {
    ++it->second;
    assert_(it->second, *occurrence);
    return 0;
  } else {
    auto it = _reported_in_file_map.find(occurrence);
    if (it != _reported_in_file_map.end()) {
      ++it->second.second;
      trace("increment existing to " << it->second.second);
      assert_(it->second.second, *occurrence);
      return it->second.first;
    }
  }
  if (_reported_in_file_map.size() < _reported_in_file_limit) {
    void *user_data = report_occurrence_in_file(occurrence);
    _reported_in_file_map.insert(
      std::make_pair(occurrence, std::make_pair(user_data, 1))
    );
    return user_data;
  } else {
    _unreported_in_file_map.insert(std::make_pair(occurrence, 1));
    return 0;
  }
}

void sa::Project::drop_tracking_in_file_user_data(Occurrence *occurrence)
{
  assert(base::is_valid(occurrence));
  trace_nest("drop occ in file " << (void*)occurrence << " " << *occurrence);
  auto it = _unreported_in_file_map.find(occurrence);
  if (it != _unreported_in_file_map.end()) {
    assert_(it->second, *occurrence);
    --it->second;
    if (!it->second) {
      _unreported_in_file_map.erase(it);
    }
  } else {
    auto it = _reported_in_file_map.find(occurrence);
    assert_(it != _reported_in_file_map.end(), *occurrence);
    --it->second.second;
    trace("decrement existing to " << it->second.second);
    if (!it->second.second) {
      withdraw_occurrence_in_file(occurrence, it->second.first);
      _reported_in_file_map.erase(it);
      // Don't check limit yet. We might be removing all tracking, and checking
      // the limit here might report occurrences that will be withdrawn later.
      //check_occurrences_in_file_limit();
    }
  }
}

size_t sa::Project::get_occurrences_of_entity_limit()
{
  return _reported_of_entity_limit;
}

void sa::Project::set_occurrences_of_entity_limit(size_t new_limit)
{
  _reported_of_entity_limit = new_limit;
  check_occurrences_of_entity_limit();
}
  
void sa::Project::check_occurrences_of_entity_limit()
{
  while (_reported_of_entity_map.size() > _reported_of_entity_limit) {
    auto it = _reported_of_entity_map.begin();
    Occurrence *occurrence = it->first;
    void *user_data = it->second;
    withdraw_occurrence_of_entity(occurrence, user_data);
    _unreported_of_entity_set.insert(occurrence);
    _reported_of_entity_map.erase(it);
  }
  while (_reported_of_entity_map.size() < _reported_of_entity_limit
    && !_unreported_of_entity_set.empty()
  ) {
    auto it = _unreported_of_entity_set.begin();
    Occurrence *occurrence = *it;
    void *user_data = report_occurrence_of_entity(occurrence);
    _reported_of_entity_map.insert( std::make_pair(occurrence, user_data) );
    _unreported_of_entity_set.erase(it);
  }
}

void *sa::Project::report_occurrence_of_entity(Occurrence *occurrence)
{
  occurrence->entity->increment_known();
  assert(occurrence->file->is_known());
  void *user_data = add_occurrence_of_entity_callback(
    occurrence->file->get_path().data(),
    occurrence->get_range().begin,
    occurrence->get_range().end,
    occurrence->kind,
    occurrence->entity->get_user_data(),
    occurrence->entity,
    occurrence->is_linked()
  );
    trace("report " << *occurrence
      << " size=" << _reported_of_entity_map.size()
      << " limit=" << _reported_of_entity_limit
    );
  return user_data;
}

void sa::Project::withdraw_occurrence_of_entity(
  Occurrence *occurrence, void *user_data
)
{
  if (remove_occurrence_of_entity_callback) {
    trace("withdraw " << *occurrence
      << " size=" << _reported_of_entity_map.size()
      << " limit=" << _reported_of_entity_limit
    );
    remove_occurrence_of_entity_callback(user_data);
  }
  occurrence->entity->decrement_known();
}

#define TRACE occurrence->entity->get_name() == "__IO" \
         && occurrence->kind == OccurrenceKind_definition
void sa::Project::add_occurrence_of_entity(Occurrence *occurrence)
{
  assert(base::is_valid(occurrence));
  assert(is_locked());
  assert(add_occurrence_of_entity_callback);
  trace_nest("add occ of entity " << (void*)occurrence << " " << *occurrence
    << " linked=" << occurrence->is_linked()
  );
  if (occurrence->kind != OccurrenceKind_use
    || _reported_of_entity_map.size() < _reported_of_entity_limit
  ) {
    void *user_data = report_occurrence_of_entity(occurrence);
    assert(
      _reported_of_entity_map.find(occurrence) == _reported_of_entity_map.end()
    );
    if (user_data) {
      _reported_of_entity_map[occurrence] = user_data;
    } else {
      occurrence->entity->decrement_known();
    }
  } else {
    _unreported_of_entity_set.insert(occurrence);
  }
}
#undef TRACE

void sa::Project::remove_occurrence_of_entity(Occurrence *occurrence)
{
  assert(base::is_valid(occurrence));
  assert(is_locked());
  trace_nest("remove occ of entity " << (void*)occurrence << " " << *occurrence);
  auto it = _unreported_of_entity_set.find(occurrence);
  if (it != _unreported_of_entity_set.end()) {
    _unreported_of_entity_set.erase(it);
  } else {
    auto it = _reported_of_entity_map.find(occurrence);
    assert(it != _reported_of_entity_map.end());
    withdraw_occurrence_of_entity(occurrence, it->second);
    _reported_of_entity_map.erase(it);
    // Don't check limit yet. We might be removing all tracking, and checking
    // the limit here might report occurrences that will be withdrawn later.
    // With the current implementation in Python, this will report to many
    // occurrences.
    trace("after check: " << _reported_of_entity_map.size());
  }
}

void sa::Project::set_occurrence_of_entity_linked(
  Occurrence *occurrence, bool linked
)
{
  assert(base::is_valid(occurrence));
  assert(is_locked());
  trace_nest("set occ of entity linked " << (void*)occurrence << " "
    << *occurrence << " " << linked
  );
  auto it = _reported_of_entity_map.find(occurrence);
  if (it != _reported_of_entity_map.end()) {
    if (set_occurrence_of_entity_linked_callback) {
      set_occurrence_of_entity_linked_callback(it->second, linked);
    }
  }
}

void sa::Project::add_missing_header(
  const std::string &header,
  Unit *unit
)
{
  assert(base::is_valid(unit));
  assert(is_locked());
  size_t &count = _missing_headers[header][unit];
  count++;
  trace("Add missing header " << header
    << " in " << unit->file->get_name()
    << " -> count=" << count
  );
  
  assert(count);
}

void sa::Project::remove_missing_header(
  const std::string &header,
  Unit *unit
)
{
  assert(base::is_valid(unit));
  assert(is_locked());
  size_t &count = _missing_headers[header][unit];
  assert(count);
  count--;
  trace("Remove missing header " << header
    << " in " << unit->file->get_name()
    << " -> count=" << count
  );
  if (!count) {
    trace("erase unit for header");
    _missing_headers[header].erase(unit);
    if (_missing_headers[header].empty()) {
      trace("erase header");
      _missing_headers.erase(header);
    }
  }
}

void sa::Project::remove_unit(Unit *unit)
{
  assert(base::is_valid(unit));
  assert(is_locked());
  trace("remove unit " << unit->file->get_name());
  unit->set_analysis_status(AnalysisStatus_none, "excluded and not up-to-date");
  flag_extractor->remove_target(unit);
}

// Check if header (specified in #include) can match path (absolute) for some
// working directory.
//
// For example: ../foo.h can match /my/abs/path/foo.h for a working directory
//   /my/abs/path/*, where * represents any directory name.
//
// TODO: move this function to filesystem path manipulation?
//
static bool header_matches_path(
  const std::string &missing_header,
  const std::string &path
)
{
  // Normalize header to get rid of things like foo/../bar, with /../ somewhere
  // in the middle.  After normalisation, ../ can only occur at the front.
  std::string normalized_header = base::get_normalized_path(missing_header);
  //
  // Remove any leading ../.  This can be matched by adding arbitrary
  // directories (except for . and ..) to the working directory.
  std::string_view header = normalized_header;
  while (header.substr(0, 3) == "../") {
    header = header.substr(3);
  }
  // The remaining header must match the tail of the path.  The working
  // directory for the match is the head of the path, plus an arbitrary name for
  // each ../ removed from the header path.
  if (path.length() <= header.length()) {
    return false;
  }
  size_t offset = path.length() - header.length();
  return path.substr(offset) == header
    && (offset == 0 || path[offset-1] == '/');
}

void sa::Project::notify_file_exists(const std::string &path)
{
  assert(is_locked());
  trace("New file: " << path);
  // Ask the flag extractor to wait; more files might be added soon.
  flag_extractor->wait();
  for (auto const &[missing_header, units]: _missing_headers) {
    trace_nest("Check missing " << missing_header);
    if (header_matches_path(missing_header, path)) {
      trace(" `-> match");

      // TODO: also check that the working directory for the match is an hdir.
      // If not,  there is stil no match.
      for (auto const &[unit, count]: units) {
        trace(count << " in " << unit->file->get_name());
        unit->trigger();
      }
    }
  }
  // Force re-run of any ongoing analysis.  The new file may match a file
  // included by the analysis, even if the analysis succeeds.  A more subtle
  // approach - checking whether there are any potential matches - is possible
  // but not implemented yet.
  //
  // TODO!!!!
  //_hdir_change_count++;
}

static bool header_exists_in_path(
  const std::string &header,
  const std::string &path
)
{
  return base::is_file(path + "/" + header);
}

void sa::Project::add_hdir(const std::string &path)
{
  assert(is_locked());
  trace_nest("Project add hdir: " << path);
  base::ptr<Hdir> hdir = get_hdir(path);
  _hdir_set.insert(hdir);
  assert(_hdir_set.find(hdir) != _hdir_set.end());
  trace(_hdir_set.size() << " hdirs");
  //
  // Touch the flag extractor, invalidating all cached extracted flags.
  // Flags include hdirs, so cached flags will change.
  flag_extractor->touch();
  //
  // Trigger units with missing headers that exist in the hdir.  Note that this
  // mechanism is currently not full-proof: a unit might already include a
  // header with the same name in another hdir, and in that case, it is not
  // triggered below, even though a re-analysis might include the header in the
  // new hdir and thus give different results. Also, the analysis of a unit
  // might still be running, so that missing headers are not known yet. TODO
  //
  // Touching the flag extractor (as done above) mitigates the issue, but also
  // causes unnecessary reanalyses.
  //
  // Touching the flag extractor does not in itself change the analysis status,
  // so a selftest like 'inline' does not wait until the new flag extraction is
  // done, so fails. For this reason, we still trigger files with missing
  // headers.
  //
  // Careful: "unit->file->notify_flags_out_of_date" might delete the unit,
  // invalidating iteration over missing headers. To avoid this issue, we create
  // a list of units to be notified first, and then iterate over it.
  //
  std::vector<base::ptr<File>> out_of_date_files;
  for (auto const &[header, units]: _missing_headers) {
    trace("check missing header " << header);
    if (header_exists_in_path(header, path)) {
      for (auto const &[unit, count]: units) {
        trace("update " << unit->file->get_name());
        (void)count;
        out_of_date_files.push_back(unit->file);
      }
    }
  }
  for (auto file: out_of_date_files) {
    file->notify_flags_out_of_date("hdir added");
  }
}

void sa::Project::remove_hdir(const std::string &path)
{
  assert(is_locked());
  trace_nest("Project remove hdir: " << path);

  // Do not use _hdir_map[path] to check if the hdir exists, because that will
  // create a hash table entry with a null pointer as a side effect if the hdir
  // doesn't exist. Other code accessing the hash table assumes that there are
  // no null pointer entries.
  std::string key = base::normalize_path_case(path);
  auto it = _hdir_map.find(key);
  if (it != _hdir_map.end()) {
    Hdir *hdir = it->second;
    assert(hdir);
    _hdir_set.erase(hdir);
    hdir->update_flags_for_dependent_units("an hdir was removed");
    trace(_hdir_set.size() << " hdirs remain");
  }
}

std::set<base::ptr<sa::Hdir>> const &sa::Project::added_hdirs() const
{
  assert(is_locked());
  return _hdir_set;
}

#ifdef MAINTAIN_TOOLCHAIN_LIST_IN_PROJECT
// Mechanism to maintain a list of toolchains (usually just one) by
// collecting information from all compilation units.  Currently not
// required, but kept in the code because it might become useful again in
// the future and is not complete trivial to implement.

void sa::Project::add_toolchain(const std::string &path)
{
  assert(is_locked());
  auto it = _toolchains.find(path);
  if (it == _toolchains.end()) {
    _toolchains.insert(std::make_pair(path, 1));
    linker->trigger();
  } else {
    ++it->second;
    assert(it->second);
  }
}

void sa::Project::remove_toolchain(const std::string &path)
{
  assert(is_locked());
  auto it = _toolchains.find(path);
  assert(it != _toolchains.end());
  assert(it->second);
  if (!it->second--) {
    _toolchains.erase(it);
    linker->trigger();
  }
}

bool sa::Project::is_toolchain_file(const File *file)
{
  assert(base::is_valid(file));
  assert(is_locked());
  for (auto it: _toolchains) {
    if (base::is_nested_in(file->get_path(), it.first)) {
      assert(it.second);
      return true;
    }
  }
  return false;
}
#endif

base::ptr<sa::Diagnostic> sa::Project::get_diagnostic(
  const std::string &message,
  Severity severity,
  Category category
)
{
  assert(is_locked());
  trace("get " << severity << ": [" << category << "] " << message);
  const DiagnosticKey key = { message, severity, category };
  auto it = _diagnostic_map.find(key);
  if (it != _diagnostic_map.end()) {
    trace("reuse");
    return it->second;
  }
  trace("create");
  auto diagnostic = base::ptr<Diagnostic>::create(
    message, severity, category, this
  );
  _diagnostic_map.insert(std::make_pair(key, diagnostic));
  return diagnostic;
}

void sa::Project::release_diagnostic(Diagnostic *diagnostic)
{
  assert(base::is_valid(diagnostic));
  assert(is_locked());
  trace("release " << diagnostic->get_severity() << ": "
    << diagnostic->get_message()
  );
  const DiagnosticKey key = {
    diagnostic->get_message(),
    diagnostic->get_severity(),
    diagnostic->get_category()
  };
  _diagnostic_map.erase(key);
}

bool sa::Project::DiagnosticKey::operator<(const DiagnosticKey &other) const
{
  if (message < other.message) return true;
  if (message > other.message) return false;
  if (severity < other.severity) return true;
  if (severity > other.severity) return false;
  return category < other.category;
}

void sa::Project::report_utf8(File const *file, bool is_valid_utf8)
{
  trace_nest("report file utf8 status " << is_valid_utf8
    << " for " << file->get_path()
  );
  assert(base::is_valid(file));
  assert(file->is_known());
  debug_lock_output;
  utf8_callback(
    file->get_path().data(),
    file->get_user_data(),
    is_valid_utf8
  );
}
