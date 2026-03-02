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

#ifndef __LinkerScriptAnalyzer_h
#define __LinkerScriptAnalyzer_h

#include "ExternalAnalyzer.h"
#include "Process.h"
#include "base/ptr.h"
#include <string>
#include <vector>

namespace sa {
  class Occurrence;
  class Diagnostic;
  
  class LinkerScriptAnalyzer: public Process, private OriginalExternalAnalyzer {
    std::vector<base::ptr<Occurrence>> new_occurrences;
    std::vector<base::ptr<Diagnostic>> new_diagnostics;
    std::set<base::ptr<File>> new_linker_scripts;
    std::vector<base::ptr<Occurrence>> occurrences;
    std::vector<base::ptr<Diagnostic>> diagnostics;
    std::set<base::ptr<File>> linker_scripts;

    std::vector<std::string> command;
  public:
    Project *const project;
    
    LinkerScriptAnalyzer(Project *project);

    ~LinkerScriptAnalyzer();

    void set_args(const std::vector<std::string>& script_analysis_args);

    const std::vector<base::ptr<Occurrence>> &get_occurrences() const
    {
      return occurrences;
    }

    // Run linker script analysis.
    void run() override;

    // Block linker while out-of-date
    void on_out_of_date() override;
    void on_up_to_date() override;

    // Increment ref count while grabbed.
    void grab() override;
    void drop() override;

    // True iff this is or is included by a linker script.
    bool is_linker_script(base::ptr<File> file);

  protected:
    void handle_stderr(std::istream &in) override;

    base::ptr<File> get_file(const std::string &path) override;

    Section *create_section(
      const std::string &name,
      const std::string &member_name = ""
    ) override;

    base::ptr<Symbol> get_global_symbol(
      const std::string &link_name
    ) override;

    base::ptr<Symbol> get_local_symbol(
      EntityKind kind,
      const std::string &user_name,
      const FileLocation &ref_location
    ) override;
    
    // Add a file inclusion occurrence.
    void add_include(
      base::ptr<File> const &included_file,
      base::ptr<File> const &including_file,
      Range range,
      const std::string &hdir_path = ""
    ) override;

    // Add a symbol occurrence.
    Occurrence *add_occurrence(
      base::ptr<Symbol> const &symbol,
      EntityKind ekind,
      OccurrenceKind okind,
      Section *section,
      base::ptr<File> const &file,
      Range range
    ) override;
    Occurrence *add_occurrence(
      base::ptr<Symbol> const &symbol,
      OccurrenceKind kind,
      OccurrenceStyle style,
      Section *section,
      base::ptr<File> const &file,
      Range range
    ) override;

    void open_scope() override;
    
    void close_scope() override;

    void report_diagnostic(
      const std::string &message,
      Severity severity,
      base::ptr<File> const &file,
      Location location
    ) override;

    void report_missing_header(const std::string &name) override;
    
    void add_memory_region(
      const std::string &name,
      size_t origin,
      size_t size
    ) override;

    struct MemoryRegion {
      size_t origin;
      size_t size;
    };

    std::map<std::string, MemoryRegion> new_memory_regions;
    std::map<std::string, MemoryRegion> memory_regions;
    
    void add_memory_section(
      const std::string &name,
      const std::string &runtime_region,
      const std::string &load_region
    ) override;

    struct MemorySection {
      std::string runtime_region;
      std::string load_region;
    };

    std::map<std::string, MemorySection> new_memory_sections;
    std::map<std::string, MemorySection> memory_sections;
  };
}

#endif
