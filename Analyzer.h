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

#ifndef __Analyzer_h
#define __Analyzer_h

#include "EntityKind.h"
#include "OccurrenceKind.h"
#include "Severity.h"
#include "File.h"
#include "Section.h"
#include "Symbol.h"
#include "Range.h"
#include "Location.h"
#include "Task.h"
#include "base/ptr.h"
#include <string>
#include <vector>
#include <set>
#include <map>

namespace sa {
  class Project;
  class Scope;
  class Occurrence;

  // Abstract interface for analyzers. An analyzer parses a source file and
  // reports symbol occurrences and diagnostics using the methods provided here.
  //
  // An analyzer is a temporary object.  Analysis results are stored in a Unit.
  // Analyzers provide a place for intermediate analysis results and auxiliary
  // methods.
  //
  class Analyzer: public base::Checked {
  public:
    Unit *const unit;
    Project *const project;

    // Create an analyzer for the given unit.
    //
    // The analyzer will report its findings to that unit, and will abort when
    // the unit - a process - is cancelled.
    Analyzer(Unit *unit);

    virtual ~Analyzer();

    // Analyze the compilation unit.
    //
    // This will call the virtual run function, which is implemented in derived
    // classes to do the actual analysis.
    //
    // The run function should use the methods defined below to report its
    // findings (files, symbols, occurrences, diagnostics, etc).  When it
    // returns, these results will be reported to the user and/or cached as
    // appropriate.
    //
    // 
    void execute(const std::string &flag_buffer);

    // Get file for given path.  Path is either absolute or relative to the
    // build path. It does not need to be normalized.
    base::ptr<File> get_file(const std::string &path);

    // Create a section for linking.  Some analyzers create multiple sections
    // (not implemented yet). Member name is used for sections of memebers of an
    // archive file.
    base::ptr<Section> create_section(
      const std::string &name,
      const std::string &member_name = ""
    );

    // Get the single section.
    Section *get_section() const;

    void add_memory_section(
      const std::string &name,
      const std::string &runtime_region,
      const std::string &load_region
    );

    void add_memory_region(
      const std::string &name,
      size_t origin,
      size_t size
    );

    // Get global symbol.
    //
    // The effective symbol kind, i.e. whether the symbol is a variable,
    // function, virtual function ... is determined by how it is used, i.e. by
    // its occurrences; entity kind is not a parameter of get_global_symbol.
    base::ptr<sa::Symbol> get_global_symbol(
      const std::string &link_name
    );

    base::ptr<sa::Symbol> get_local_symbol(
      EntityKind kind,
      const std::string &user_name,
      const FileLocation &ref_location
    );
    
    // Add a file inclusion occurrence.  Hdir path is either absolute or
    // relative to the build path. It does not need to be normalized.
    void add_include(
      base::ptr<File> const &included_file,
      base::ptr<File> const &including_file,
      Range range,
      const std::string &hdir_path = ""
    );

    // Add a symbol occurrence.
    Occurrence *add_occurrence(
      base::ptr<Symbol> const &symbol,
      EntityKind ekind,
      OccurrenceKind okind,
      Section *section,
      base::ptr<File> const &file,
      Range range,
      bool add_to_section
    );
    Occurrence *add_occurrence(
      base::ptr<Symbol> const &symbol,
      OccurrenceKind kind,
      OccurrenceStyle style,
      Section *section,
      base::ptr<File> const &file,
      Range range,
      bool add_to_section
    );
    void add_global_occurrence_to_section(
      sa::Occurrence *occurrence,
      Section *section
    );

    // For GDB workaround
    void add_empty_loop(
      base::ptr<File> const &file,
      Range range
    );

    // Require a definition of a global symbol, even if the symbol is not used
    // directly at a specific location in the source code.  An example is a
    // virtual function of (a base class of) a class that is instantiated in the
    // source code,  or a call of an instantiation of a function template.
    void require_definition(
      base::ptr<Symbol> const &symbol,
      Section *section
    );

    // Weakly require a definition of a global symbol.
    void weakly_require_definition(
      base::ptr<Symbol> const &symbol,
      Section *section
    );

    // Provide a weak definition of a global symbol, even if the symbol is not
    // defined at a specific location in the source code.
    //
    // An example is a method of a class template that is implicitly
    // instantiated with specific template parameter values. This causes the
    // compiler to emit a weak definition of a global function, at no specific
    // location in the source file. More precisely, there are several equivalent
    // approaches that the compiler can use, and emitting a weak definition is
    // one of them.
    //
    // Calls to a function template (such as a method of a class template)
    // should be treated as uses of the function template for source
    // cross-referencing purposes.
    void provide_weak_definition(
      base::ptr<Symbol> const &symbol,
      Section *section
    );

    // Open a scope for the most recently added occurrence.
    //
    // Symbols and symbol occurrences added while the scope is open will be
    // nested in the scope of that definition.
    //
    // At most one scope can be opened per occurrence, and can_be_scope() must
    // return true for that occurrence.
    //
    // Multiple scopes can be open at the same time.  Symbols and occurrences
    // are added to the most recently opened scope that was not closed yet.
    //
    void open_scope();
    
    // Close the most recently opened scope that was not closed yet.  Every
    // opened scope must be closed.
    void close_scope();

    // Return current scope,  or null at file scope.
    Occurrence *current_scope() const;

    // Return top level scope,  or null at file scope.
    Occurrence *top_scope() const;

    // File can be null for no location
    void report_diagnostic(
      const std::string &message,
      Severity severity,
      base::ptr<File> const &file,
      Location location,
      bool is_linker_relevant
    );

    void report_diagnostic(
      const std::string &message,
      Severity severity,
      FileLocation location,
      bool is_linker_relevant
    );

    void report_missing_header(
      const std::string &name
    );

    void set_alternative_content(const std::string &content)
    {
      has_alternative_content = true;
      alternative_content = content;
    }

    // Report the presence of at least one non-UTF-8 character in a file.
    // Additional calls for the same file have no effect.
    void report_non_utf8_file(base::ptr<File> const &file);

  protected:
    
    // Run analyzer. Return true on success and false on failure.
    // An analyzer that finds errors in the source code is still successful.
    virtual bool run(
      const std::string &flag_buffer
    ) = 0;

  private:

    // Return path of cache file.
    std::string get_cache_path() const;

    // Write analysis data to cache.
    void write_cache(std::string const &flag_buffer);

    void write_cache_occurrence(
      Occurrence *occurrence,
      std::map<Symbol*, unsigned> &symbols,
      std::map<Occurrence*, unsigned> &scopes,
      std::map<File*, unsigned> const &files,
      std::map<Hdir*, unsigned> const &hdirs,
      unsigned &next_scope_index,
      std::ofstream &out
    );
    
    unsigned write_cache_symbol(
      Symbol *symbol,
      std::map<Symbol*, unsigned> &symbols,
      std::map<Occurrence*, unsigned> &scopes,
      std::map<File*, unsigned> const &files,
      std::ofstream &out
    );

    // Read analysis data from cache, return true iff success.
    // Fail if cached flags do not match expected flags.
    bool read_cache(const std::string &expected_flags);

    bool read_cache_core(
      const std::string &expected_flags,
      std::vector<base::ptr<File>> &files,
      std::vector<base::ptr<sa::Hdir>> &hdirs,
      std::vector<base::ptr<Symbol>> &symbols
    );

    bool read_cache_symbol(
      std::istream &in,
      std::vector<base::ptr<Symbol>> &symbols,
      std::vector<base::ptr<File>> &files,
      std::vector<Occurrence*> &scopes
    );
    
    void push_diagnostic(Diagnostic *diagnostic, bool is_linker_relevant);

  private:
    std::string toolchain;
    std::vector<std::string> compiler_flags;
    std::vector<std::string> analysis_flags;
    
    std::string cache_path;

    Occurrence *current_occurrence = 0;
    std::vector<Scope*> scope_stack;
    std::vector<base::ptr<Occurrence>> parent_stack;

    std::string alternative_content;
    bool has_alternative_content = false;

    //--------------------------------------------------------------------------
    // Analysis data:
    //--------------------------------------------------------------------------

    // Unit can access analysis data.
    friend class Unit;
    
    bool success = false;
    
    // A linker relevant error is an error that affects the occurrence of global
    // symbols in the compilation unit.  This flag is set if there is at least
    // one linker relevant error.
    bool has_linker_relevant_error = false;

    std::vector<base::ptr<Occurrence>> occurrences;
    std::map<Occurrence*, sa::Scope> scope_data;
    std::vector<base::ptr<Diagnostic>> diagnostics;
    std::vector<std::string> missing_headers;
    std::vector<base::ptr<Section>> sections;
    std::vector<base::ptr<EmptyLoop>> empty_loops;
    std::set<base::ptr<File>> non_utf8_files;
    bool from_cache = false;
  };
}

#endif
