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

#ifndef __Unit_h
#define __Unit_h

#include "Process.h"
#include "FileMode.h"
#include "AnalysisStatus.h"
#include "InclusionStatus.h"
#include "Section.h"
#include "EmptyLoop.h"
#include "Scope.h"
#include "Diagnostic.h"
#include "FlagInfo.h"
#include "compiler.h"
#include "base/ptr.h"
#include <vector>
#include <map>
#include <set>

namespace sa {
  class File;
  class Linker;
  class Project;
  class Diagnostic;
  class FlagInfo;

  // A compilation unit analyzes its main file - presumably a C or C++ file - in
  // the background and annotates symbol occurrences on the main file and any
  // include files.
  //
  // Occurrences in include files can be shared between multiple compilation
  // units. They are reference-counted and will be deleted as soon as the
  // reference count reaches zero.
  //
  // Units exist for any linkable file: C, C++, asm, archive and object.
  //
  class Unit: public Process
  {
    FileMode mode;
    std::string compiler;
    std::string toolchain;
    CompilerResults const *compiler_results = 0;
    std::vector<std::string> compiler_flags;
    std::vector<std::string> analysis_flags;
    std::vector<base::ptr<Diagnostic>> analysis_flag_diagnostics;

    // For source files that need to be compiled, true iff analysis flags are
    // valid.  For other files (binary, archive), always true.
    bool analysis_flags_valid;

    // True iff analysis data was loaded from cache. Used in old caching code in
    // Analyzer.cpp.
    bool _from_cache = false;

    // True iff cache reading has been tried (and either succeeded or failed).
    // When set, do not try to read cache again. Used in new caching code.
    bool cache_tried = false;

    // Empty loops of this unit
    std::vector<base::ptr<EmptyLoop>> _empty_loops;

    // Sections of this unit
    std::vector<base::ptr<Section>> _sections;

    // List of all occurrences found in this compilation unit.
    std::vector<base::ptr<Occurrence>> _occurrences;

    // Scope data is stored in a map with an (index,count) pair for each
    // occurrence with nested occurrences.  Nested occurrences are always stored
    // immediately after their parent occurrence in the _occurrences list above.
    // The 'index' is the position of the first nested occurrence, and the
    // 'count' is the number of nested occurrences.
    //
    // Note that scope data cannot be stored in the occurrence itself, because
    // an occurrence in a header file might be instantiated in more than one
    // unit, with different scope data for each unit.
    std::map<Occurrence*, sa::Scope> _scope_data;

    // List of all diagnostics found in this compilation unit.
    std::vector<base::ptr<Diagnostic>> _diagnostics;

    // List of missing header files for this compilation unit.
    std::vector<std::string> _missing_headers;

    // Non-utf8 files detected in this unit
    std::set<base::ptr<File>> _non_utf8_files;

    // True iff an error was found that may affect occurrences of global symbols.
    bool _has_linker_relevant_error = false;

    // The number of reasons why the unit should be linked when in automatic
    // mode: a section is included, or there is a linker-relevant error.
    unsigned _soft_link_count = 0;

    // The number of reasons why the unit must be linked, even if it is
    // force-excluded: it is mentioned on the link command line, or it is
    // implicitly included by the linker.
    unsigned _hard_link_count = 0;

    // The analysis status of the most recent analysis. This can be none, error
    // or done, but never waiting or busy. It is updated together with the
    // analysis results in swap_analysis_results.
    AnalysisStatus _analysis_status = AnalysisStatus_none;

  public:
    base::ptr<File> const file;
    Project *const project;
    Linker *const linker;
    
    Unit(File *file);
    ~Unit();

    // Get the list of sections in this compilation unit.  The project must be
    // locked (asserted) when calling this, and the list can change as soon as
    // the project is unlocked.
    const std::vector<base::ptr<Section>> &get_sections() const;

    void set_mode(FileMode mode);
    FileMode get_mode() const { return mode; }

    void run() override;
    void on_status(Status status) override;
    void on_up_to_date() override;
    void on_out_of_date() override;
    void grab() override;
    void drop() override;

    bool was_read_from_cache() const { return _from_cache; }

    // Update analysis flags and unblock process. If flags have changed, either
    // trigger a re-analysis of the unit and unblock it, or delete it (for
    // force-excluded files). If flags have not changed, just unblock it.
    void update_analysis_flags_and_unblock(sa::FlagInfo *flag_info);

    void set_analysis_status(AnalysisStatus status, const char *reason);
    AnalysisStatus get_analysis_status() const { return _analysis_status; }
    const std::string &get_compiler() { return compiler; }
    const std::string &get_toolchain() { return toolchain; }

    bool cache_loaded = false;

    // Swap lists of occurrences and diagnostics and set analysis status.
    void swap_analysis_results(
      bool has_linker_relevant_error,
      std::vector<base::ptr<Occurrence>> &occurrences,
      std::map<Occurrence*, sa::Scope> scope_data,
      std::vector<base::ptr<Diagnostic>> &diagnostics,
      std::vector<std::string> &missing_headers,
      std::vector<base::ptr<Section>> &sections,
      std::vector<base::ptr<EmptyLoop>> &empty_loops,
      std::set<base::ptr<File>> non_utf8_files,
      bool from_cache
    );

    // Set alternative content
    void set_alternative_content(const std::string &content);

    // Return true iff the unit is linked into the elf file.  A unit is linked
    // into the elf file if it is in include mode, or if it is in automatic mode
    // and at least one section is included, or if it has a linker relevant
    // error and is not in exclude or header mode, or if it is explicitly
    // mentioned on the command line (not via filetree.mk), or if it is
    // implicitly included by the linker.
    bool is_linked() const;

    // Return true iff this file occurs in the link command, even if not added
    // via filetree.mk.  Such files are always linked, even if in exclude mode.
    bool is_in_link_command() const;

    // Change the soft link count. The soft link count is the number of reasons
    // why the unit should be linked when in automatic mode: a section is
    // included, or there is a linker-relevant error.
    void inc_soft_link_count();
    void dec_soft_link_count();

    // Change the hard link count. The hard link count is the number of reasons
    // why the unit should be linked, even if it is force-excluded: it is
    // mentioned on the link command line, or it is implicitly included by the
    // linker.
    void inc_hard_link_count();
    void dec_hard_link_count();

    // Return true iff the current analysis has one or more errors that may
    // affect the occurrences of global symbols.
    bool has_linker_relevant_error() const { return _has_linker_relevant_error; }

#ifndef NDEBUG
    std::string get_debug_name() const override;
#endif
    
  protected:

    // Propagate a new link status to all involved files and occurrences.  This
    // method blindly applies the new link status. Specifically, it does
    // *not* take into account the current result of should_link or the current
    // mode. It is the responsibility of the caller to make sure that the new
    // link status is correct.
    void set_linked(bool status);

    // Increment the instantiation count of occurrences, diagnostics and missing
    // headers in this compilation unit with the specified link status.
    void increment_instantiation_count(
      bool linked,
      std::vector<base::ptr<Occurrence>> &occurrences,
      std::vector<base::ptr<Diagnostic>> &diagnostics,
      std::vector<std::string> &missing_headers,
      std::vector<base::ptr<EmptyLoop>> &empty_loops,
      std::set<base::ptr<File>> non_utf8_files
    );
    
    // Decrement the instantiation count of occurrences, diagnostics and missing
    // headers in this compilation unit with the specified link status.
    void decrement_instantiation_count(
      bool linked,
      std::vector<base::ptr<Occurrence>> &occurrences,
      std::vector<base::ptr<Diagnostic>> &diagnostics,
      std::vector<std::string> &missing_headers,
      std::vector<base::ptr<EmptyLoop>> &empty_loops,
      std::set<base::ptr<File>> non_utf8_files
    );

  private:
#ifdef CHECK
    void notify_ref_count() const override;
#endif
  };
}

#endif
