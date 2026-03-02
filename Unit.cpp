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

#include "Unit.h"
#include "File.h"
#include "FlagExtractor.h"
#include "Linker.h"
#include "Unit.h"
#include "Project.h"
#include "Clang.h"
#include "AsmAnalyzer.h"
#include "BinaryAnalyzer.h"
#include "Diagnostic.h"
#include "Occurrence.h"
#include "UnitResults.h"
#include "compiler.h"
#include "cache.h"
#include "base/filesystem.h"
#include "base/os.h"
#include "base/debug.h"
#include <sstream>

sa::Unit::Unit(File *file)
  : Process(file->get_name())
  , mode(FileMode_exclude)
  , analysis_flags_valid(!file->is_source_file())
  , file(file)
  , project(file->project)
  , linker(project->linker)
{
  trace_nest("Create Unit " << process_name());
  assert(project->is_locked());
  assert(file->has_linkable_file_kind());

  // Flag extractor will trigger flag extraction when appropriate.
  // The target will be blocked until flag extraction is done.
  project->flag_extractor->add_target(this);
  
  // Trigger this unit to make sure it will run when unblocked, even if flags
  // did not change. Triggering now is essential for automatic and
  // force-included files, because triggering a unit blocks the linker until the
  // analysis is done.
  trigger();

  // Increment instantiation count with current link status.  This is not
  // strictly necessary, as it will only do something when one of the lists is
  // non-empty, and all lists are initially empty.  We do it anyway for symmetry
  // reasons, and to be robust in case the implementation changes.
  increment_instantiation_count(
    is_linked(), _occurrences, _diagnostics, _missing_headers, _empty_loops,
    _non_utf8_files
  );
}

sa::Unit::~Unit()
{
  trace_nest("Destroy Unit " << process_name());
  assert(project->is_locked());
  assert_(is_up_to_date(), process_name());

  // Setting mode to header is not strictly necessary as the unit will be
  // destroyed anyway, but it is a convenient way to remove the unit from the
  // linker and trigger the linker if needed.
  if (mode != FileMode_exclude) {
    set_mode(FileMode_exclude);
  }
  // Decrement instantiation count at current level.  This makes sure that
  // occurrences, diagnostics and missing headers in this unit are no longer
  // active.
  decrement_instantiation_count(
    is_linked(), _occurrences, _diagnostics, _missing_headers, _empty_loops,
    _non_utf8_files
  );

  // Remove linked sections from the linker. The linker keeps a list of linked
  // sections, and these sections will be deleted when their unit is deleted.
  // If we don't remove the sections from the linker here, they will become
  // dangling pointers.
  for (Section *section: _sections) {
    if (!_soft_link_count) break;
    if (section->is_linked()) {
      linker->drop_section(section);
    }
  }
}

const std::vector<base::ptr<sa::Section>> &sa::Unit::get_sections() const
{
  assert(project->is_locked());
  return _sections;
}

void sa::Unit::set_mode(FileMode new_mode)
{
  assert(project->is_locked());
  Lock lock(this);
  trace_nest("set mode from '" << mode << "' to '" << new_mode
    << "' for unit " << process_name()
  );
  if (new_mode != mode) {
    // Update linker list of forced files, trigger linker if necessary.
    // Update link status if new mode forces a different status.  The linker
    // will only update the inclusion status of automatic files.
    Lock lock(linker);

    // Check old link status before changing anything that can affect it.
    bool ori_linked = is_linked();

    // Update linker list of forced files
    if (new_mode == FileMode_include) {
      linker->_add_forced_file(file);
    }
    if (mode == FileMode_include) {
      linker->_remove_forced_file(file);
    }
    
    // Update mode
    {
      bool old_linked = is_linked();
      mode = new_mode;
      bool new_linked = is_linked();
      if (new_linked != old_linked) {
        set_linked(new_linked);
      }
    }
    _set_urgent(mode != FileMode_exclude);

    // Trigger the linker, unless:
    //   - new mode is force-include, and this unit was already linked, or
    //   - new mode is force-exclude, and this unit was not linked
    if (!(
        (new_mode == FileMode_include && ori_linked) ||
        (new_mode == FileMode_exclude && !ori_linked)
      )) {
      trace("trigger linker");
      linker->_trigger();
    }
  }
}

void sa::Unit::on_out_of_date()
{
  assert(is_locked());
  trace_nest(process_name() << " report out-of-date");
  linker->block();
  set_analysis_status(AnalysisStatus_waiting, "analysis triggered");
}

void sa::Unit::on_status(Status status)
{
  trace(process_name() << " status: " << status);
  // Update file analysis status in the run method, where the project is
  // locked. Cannot lock the project here, because the process is already
  // locked.
}

void sa::Unit::on_up_to_date()
{
  assert(is_locked());
  trace_nest(process_name() << " report up-to-date");
  // File analysis status update is in the run method, where the project is
  // still locked.
  linker->unblock();
}

void sa::Unit::grab()
{
  trace_nest("grab " << process_name());
  file->increment_ref_count();
}

void sa::Unit::drop()
{
  trace_nest("drop " << process_name());
  Lock project_lock(project);
  file->decrement_ref_count();
}

void sa::Unit::run()
{
  trace_nest("Unit::run " << process_name());
  // Do code analysis ...  lock to get flags

  std::string flag_buffer;
  bool try_cache = false;
  std::string frozen_compiler;
  {
    Lock project_lock(project);
    Lock lock(this);
    if (cancelled()) {
      trace("Analyzer " << process_name() << " was cancelled");
      return;
    }
    set_analysis_status(AnalysisStatus_busy, "analysis starts");
    if (!analysis_flags_valid
      || (compiler_results && !compiler_results->valid)
    ) {
      assert(file->is_source_file()); // Binary files do not have flags
      trace_nest("Report failed flag extraction:");
      for (auto d: analysis_flag_diagnostics) {
        trace(*d);
      }
      {
        // Replace existing analysis results with flag extraction diagnostics
        std::vector<base::ptr<Occurrence>> occurrences;
        std::map<Occurrence*, sa::Scope> scope_data;
        std::vector<base::ptr<Diagnostic>> diagnostics(
          analysis_flag_diagnostics
        );
        if (compiler_results && !compiler_results->valid) {
          for (auto &cd: compiler_results->diagnostics) {
            diagnostics.push_back(
              project->get_diagnostic(cd.message, cd.severity, cd.category)
            );
          }
        }
        std::vector<std::string> missing_headers;
        std::vector<GlobalSymbol*> required_globals;
        std::vector<base::ptr<Section>> sections;
        std::vector<base::ptr<EmptyLoop>> empty_loops;
        std::set<base::ptr<File>> non_utf8_files;
        swap_analysis_results(
          true,
          occurrences,
          scope_data,
          diagnostics,
          missing_headers,
          sections,
          empty_loops,
          non_utf8_files,
          false
        );
      }
      set_analysis_status(AnalysisStatus_failed,"flag extraction failed");
      return;
    }
    if (!cache_tried) {
      cache_tried = true;
      try_cache = true;
    }
    frozen_compiler = compiler;
    if (file->is_source_file()) {
      assert(compiler_results && compiler_results->valid);
      for (auto &flag: compiler_results->builtin_flags) {
        flag_buffer.append(flag.data(), flag.size()+1);
      }
    }
    for (auto &flag: analysis_flags) {
      flag_buffer.append(flag.data(), flag.size()+1);
    }
  }
  // Execute with project unlocked
  UnitResults results;
  if (!(try_cache && read_cache(flag_buffer))) {
    trace("cache invalid so run analyzer")
    Analyzer *analyzer = 0;
    switch (file->file_kind) {
      case FileKind_c:
      case FileKind_cplusplus:
        analyzer = new Clang(
          this, frozen_compiler, project->get_clang_resource_path()
        );
        break;
      case FileKind_assembler_with_cpp:
        // Fall through
      case FileKind_assembler:
        analyzer = new AsmAnalyzer(this);
        break;
      case FileKind_object:
      case FileKind_archive:
        analyzer = new BinaryAnalyzer(this);
        break;
      default:
        assert(false);
        return;
    };
    analyzer->execute(flag_buffer);
    std::swap(results.success, analyzer->success);
    std::swap(results.occurrences, analyzer->occurrences);
    std::swap(results.diagnostics, analyzer->diagnostics);
    std::swap(
      results.has_linker_relevant_error,
      analyzer->has_linker_relevant_error
    );
    std::swap(results.scope_data, analyzer->scope_data);
    std::swap(results.missing_headers, analyzer->missing_headers);
    std::swap(results.sections, analyzer->sections);
    std::swap(results.empty_loops, analyzer->empty_loops);
    std::swap(results.non_utf8_files, analyzer->non_utf8_files);
    std::swap(results.from_cache, analyzer->from_cache);
    std::swap(
      results.has_alternative_content,
      analyzer->has_alternative_content
    );
    std::swap(results.alternative_content, analyzer->alternative_content);
    trace("delete analyzer");
    delete analyzer;
  }
  // Apply results
  Lock project_lock(project);
  Lock lock(this);
  if (cancelled()) {
    trace("Analyzer for " << process_name() << " cancelled");
    // Clear results while project is locked
    results = UnitResults();
    return;
  }
  if (results.has_alternative_content) {
    set_alternative_content(results.alternative_content);
  }
  swap_analysis_results(
    results.has_linker_relevant_error,
    results.occurrences,
    results.scope_data,
    results.diagnostics,
    results.missing_headers,
    results.sections,
    results.empty_loops,
    results.non_utf8_files,
    results.from_cache
  );
  trace("Analyze " << process_name() << " done  mode=" << mode
    << " is-in-link-command=" << is_in_link_command()
    << " analysis status=" << get_analysis_status()
  );
  if ( mode != FileMode_exclude || is_in_link_command()) {
    linker->trigger();
  }
  set_analysis_status(
    results.success ? AnalysisStatus_done : AnalysisStatus_failed,
    "analysis stops"
  );
  // Clear results while project is locked
  results = UnitResults();
}

void sa::Unit::update_analysis_flags_and_unblock(FlagInfo *flag_info)
{
  trace_nest("Unit::update_analysis_flags_and_unblock " << process_name());
  assert(file->is_source_file());
  const char *trigger_reason = 0;
  if (!flag_info->valid) {
    if (analysis_flags_valid) {
      trace("flag info not valid, " << flag_info->diagnostics.size()
        << " diagnostics: " << flag_info->diagnostics
      );
      analysis_flag_diagnostics = flag_info->diagnostics;
      trigger_reason = "flag info becomes invalid";
      analysis_flags_valid = false;
    }
    if (analysis_flag_diagnostics != flag_info->diagnostics) {
      trace("update flag diagnostics");
      analysis_flag_diagnostics = flag_info->diagnostics;
      trigger_reason = "flag diagnostics changed";
    }
  } else {
    trace("flag info valid");
    if (!analysis_flags_valid) {
      trace("flag diagnostics cleared");
      analysis_flag_diagnostics.clear();
      trigger_reason = "flag info becomes valid";
      analysis_flags_valid = true;
    }
    if (flag_info->compiler != compiler) {
      trace("compiler changed from " << compiler << " to "
        << flag_info->compiler
      );
      compiler = flag_info->compiler;
      //assert(base::is_absolute_path(compiler));
      std::string toolchain =
        base::get_parent_path(base::get_parent_path(compiler));
      if (toolchain != this->toolchain) {
#ifdef MAINTAIN_TOOLCHAIN_LIST_IN_PROJECT
        if (_inclusion_status != InclusionStatus_excluded) {
          // Mechanism to maintain a list of toolchains (usually just one) by
          // collecting information from all compilation units.  Currently not
          // required, but kept in the code because it might become useful again
          // in the future and is not completely trivial to implement.
          if (!toolchain.empty()) {
            project->add_toolchain(toolchain);
          }
          if (!this->toolchain.empty()) {
            project->remove_toolchain(_toolchain);
          }
        }
#endif
        this->toolchain = toolchain;
      }
      trace("trigger " << process_name());
      trigger_reason = "compiler changed";
      compiler_results = get_compiler_builtin_flags(
        compiler, file->file_kind, flag_info->compiler_flags
      );
    }
    if (compiler_flags != flag_info->compiler_flags) {
      trace("compiler flags changed ("
        << flag_info->compiler_flags.size() << " new flags)"
      );
      compiler_flags = flag_info->compiler_flags;
      trigger_reason = "compiler flags changed";
    }
    if (analysis_flags != flag_info->analysis_flags) {
      trace("analysis flags changed ("
        << flag_info->analysis_flags.size() << " new flags)"
      );
      analysis_flags = flag_info->analysis_flags;
      trigger_reason = "analysis flags changed";
    }
    if (!trigger_reason) {
      trace("compiler and flags unchanged");
      assert(base::is_valid(file));
      // If the file has not been triggered for any other reason, set its
      // analysis status.
      if (is_up_to_date()) {
        set_analysis_status(AnalysisStatus_done, "flags unchanged");
      }
    } else {
      // Compatibility: re-compute flag buffer.
    }
  }
  if (trigger_reason) {
    // This call might delete the unit, so it must be last!
    file->notify_out_of_date_and_unblock_unit(trigger_reason);
  } else {
    unblock();
  }
}

void sa::Unit::set_analysis_status(AnalysisStatus status, const char *reason)
{
  trace("Unit::set_analysis_status of " << process_name() << " to " << status
    << " because " << reason
  );
  assert(project->is_locked());
  if (_analysis_status != status) {
    AnalysisStatus old_status = _analysis_status;
    _analysis_status = status;
    project->update_for_analysis_status_change(
      this, old_status, status, reason
    );
  }
}

void sa::Unit::swap_analysis_results(
  bool has_linker_relevant_error,
  std::vector<base::ptr<Occurrence>> &occurrences,
  std::map<Occurrence*, sa::Scope> scope_data,
  std::vector<base::ptr<Diagnostic>> &diagnostics,
  std::vector<std::string> &missing_headers,
  std::vector<base::ptr<Section>> &sections,
  std::vector<base::ptr<EmptyLoop>> &empty_loops,
  std::set<base::ptr<File>> non_utf8_files,
  bool from_cache
)
{
  assert(project->is_locked());
  trace_nest("swap analysis results for " << process_name());
  _from_cache = from_cache;
  //for (auto section: _sections) {
  //  trace("old section " << *section);
  //}
  //for (auto section: sections) {
  //  trace("new section " << *section);
  //}
  trace("old linked=" << is_linked());
  trace("has linker relevant error = " << has_linker_relevant_error);
  trace("#occurrences " << _occurrences.size() << " -> " << occurrences.size());
  trace("#diagnostics " << _diagnostics.size() << " -> " << diagnostics.size());
  // Update instantiation counts even if inclusion status did not change: the
  // instantiated occurrences, diagnostics and missing headers have changed.
  // In other words: we cannot use set_linked(...) here.
  bool old_linked = is_linked();
  _has_linker_relevant_error = has_linker_relevant_error;
  bool new_linked = is_linked();
  trace("is linked " << old_linked << " -> " << new_linked);
  if (new_linked != old_linked) {
    if (new_linked) {
      file->inc_link_count();
    } else {
      file->dec_link_count();
    }
  }
  increment_instantiation_count(
    new_linked, occurrences, diagnostics, missing_headers, empty_loops,
    non_utf8_files
  );
  decrement_instantiation_count(
    old_linked, _occurrences, _diagnostics, _missing_headers, _empty_loops,
    _non_utf8_files
  );
  swap(_empty_loops, empty_loops);
  swap(_sections, sections);
  swap(_occurrences, occurrences);
  swap(_scope_data, scope_data);
  swap(_diagnostics, diagnostics);
  swap(_missing_headers, missing_headers);

#if 0
  for (auto item: _scope_data) {
    trace("scope data: " << *item.first
      << " " << item.second.first
      << " " << item.second.second
    );
  }
  for (auto occurrence: _occurrences) {
    show_tree(this, occurrence);
  }
#endif
  trace("got " << _diagnostics.size() << " diagnostics");
}

void sa::Unit::set_alternative_content(const std::string &content)
{
  project->set_alternative_content(file, content.data());
}

bool sa::Unit::is_linked() const
{
  // Careful: is_linked is used in trace statements, so tracing both here and
  // there causes a deadlock.
  //trace("mode=" << mode
  //  << " hard_links=" << _hard_link_count
  //  << " soft_links=" << _soft_link_count
  //  << " linker_relevant_error=" << _has_linker_relevant_error
  //);
  return _hard_link_count || mode == FileMode_include
    || (mode == FileMode_automatic
      && (_soft_link_count || _has_linker_relevant_error)
    );
}

bool sa::Unit::is_in_link_command() const
{
  return _hard_link_count;
}

void sa::Unit::inc_soft_link_count()
{
  trace_nest("inc soft link count from " << _soft_link_count
    << " for unit " << process_name() << " " << mode << " " << _hard_link_count
  );
  bool old_linked = is_linked();
  ++_soft_link_count;
  assert(_soft_link_count);
  bool new_linked = is_linked();
  if (new_linked != old_linked) {
    assert(new_linked);
    set_linked(true);
  }
}

void sa::Unit::dec_soft_link_count()
{
  bool old_linked = is_linked();
  assert(_soft_link_count);
  --_soft_link_count;
  trace_nest("dec soft link count to " << _soft_link_count
    << " for unit " << process_name() << " " << mode << " " << _hard_link_count
  );
  bool new_linked = is_linked();
  if (new_linked != old_linked) {
    assert(!new_linked);
    set_linked(false);
  }
}

void sa::Unit::inc_hard_link_count()
{
  trace_nest("inc hard link count from " << _hard_link_count
    << " for unit " << process_name() << " " << mode << " " << _soft_link_count
  );
  if (!is_linked()) {
    set_linked(true);
  }
  bool old_link_candidate = file->is_link_candidate();
  ++_hard_link_count;
  if (!old_link_candidate) {
    bool new_link_candidate = file->is_link_candidate();
    if (new_link_candidate) {
      project->update_link_candidate_status(
        file, true, get_analysis_status()
      );
    }
  }
  assert(_hard_link_count);
}

void sa::Unit::dec_hard_link_count()
{
  bool old_link_candidate = file->is_link_candidate();
  assert(_hard_link_count);
  --_hard_link_count;
  trace_nest("dec hard link count to " << _hard_link_count
    << " for unit " << process_name() << " " << mode << " " << _soft_link_count
  );
  if (old_link_candidate) {
    bool new_link_candidate = file->is_link_candidate();
    if (!new_link_candidate) {
      project->update_link_candidate_status(
        file, false, get_analysis_status()
      );
    }
  }
  if (!is_linked()) {
    set_linked(false);
  }
}

void sa::Unit::set_linked(bool linked)
{
  trace_nest("set linked=" << linked << " for unit " << process_name());
  if (linked) {
    file->inc_link_count();
  } else {
    file->dec_link_count();
  }    
  increment_instantiation_count(
    linked, _occurrences, _diagnostics, _missing_headers, _empty_loops,
    _non_utf8_files
  );
  decrement_instantiation_count(
    !linked, _occurrences, _diagnostics, _missing_headers, _empty_loops,
    _non_utf8_files
  );
}

void sa::Unit::increment_instantiation_count(
  bool linked,
  std::vector<base::ptr<Occurrence>> &occurrences,
  std::vector<base::ptr<Diagnostic>> &diagnostics,
  std::vector<std::string> &missing_headers,
  std::vector<base::ptr<EmptyLoop>> &empty_loops,
  std::set<base::ptr<File>> non_utf8_files
)
{
  assert(project->is_locked());
  trace_nest("increment instantiation count at " << linked << " for "
    << process_name()
    << " (" << occurrences.size() << " occurrences)"
    << " (" << diagnostics.size() << " diagnostics)"
  );
  for (auto occurrence: occurrences) {
    // Trace below produces a lot of output for non-toy examples.
    //trace_nest("insert " << linked << " instance " << *occurrence);
    occurrence->insert_instance(linked);
  }
  // Include a virtual instance of the main file. This symbolizes the fact that
  // the main file is part of an included compilation unit, even though it is
  // not explicitly #included anywhere in the code and there is no corresponding
  // occurrence.
  //file->insert_unit(linked);
  for (auto missing_header: missing_headers) {
    trace("add missing header " << missing_header);
    project->add_missing_header(missing_header, this);
  }
  for (auto empty_loop: empty_loops) {
    trace("add empty loop " << empty_loop);
    empty_loop->insert_instance();
  }
  for (auto file: non_utf8_files) {
    file->increment_non_utf8();
  }
  if (linked) {
    project->add_unit_diagnostics(diagnostics);
#ifdef MAINTAIN_TOOLCHAIN_LIST_IN_PROJECT
    // Mechanism to maintain a list of toolchains (usually just one) by
    // collecting information from all compilation units.  Currently not
    // required, but kept in the code because it might become useful again in
    // the future and is not completely trivial to implement.
    if (!_toolchain.empty()) {
      project->add_toolchain(_toolchain);
    }
#endif
  }
}

void sa::Unit::decrement_instantiation_count(
  bool linked,
  std::vector<base::ptr<Occurrence>> &occurrences,
  std::vector<base::ptr<Diagnostic>> &diagnostics,
  std::vector<std::string> &missing_headers,
  std::vector<base::ptr<EmptyLoop>> &empty_loops,
  std::set<base::ptr<File>> non_utf8_files
)
{
  assert(project->is_locked());
  trace_nest("decrement instantiation count at " << linked << " for "
    << process_name()
    << " (" << occurrences.size() << " occurrences)"
    << " (" << diagnostics.size() << " diagnostics)"
  );
  if (linked) {
#ifdef MAINTAIN_TOOLCHAIN_LIST_IN_PROJECT
    // Mechanism to maintain a list of toolchains (usually just one) by
    // collecting information from all compilation units.  Currently not
    // required, but kept in the code because it might become useful again in
    // the future and is not completely trivial to implement.
    if (!_toolchain.empty()) {
      project->remove_toolchain(_toolchain);
    }
#endif
    project->remove_unit_diagnostics(diagnostics);
  }
  for (auto file: non_utf8_files) {
    file->decrement_non_utf8();
  }
  for (auto empty_loop: empty_loops) {
    trace("remove empty loop " << empty_loop);
    empty_loop->remove_instance();
  }
  for (auto missing_header: missing_headers) {
    trace("remove missing header " << missing_header);
    project->remove_missing_header(missing_header, this);
  }
  // Exclude a virtual instance of the main file. This symbolizes the fact
  // that the main file is part of an included compilation unit, even though
  // it is not explicitly #included anywhere in the code and there is no
  // corresponding occurrence.
  //file->remove_unit(linked);
  for (auto occurrence: occurrences) {
    // Trace below produces a lot of output for non-toy examples.
    //trace_nest("remove " << linked << " instance " << *occurrence);
    occurrence->remove_instance(linked);
  }
}

#ifndef NDEBUG
std::string sa::Unit::get_debug_name() const
{
  return process_name();
}
#endif

#ifdef CHECK
void sa::Unit::notify_ref_count() const
{
  trace("Set ref count for unit " << process_name()
    << " to " << get_ref_count()
  );
}
#endif
