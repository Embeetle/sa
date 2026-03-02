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

#ifndef __Linker_h
#define __Linker_h

#include "LinkerScriptAnalyzer.h"
#include "Task.h"
#include "Process.h"
#include "LinkerStatus.h"
#include "LinkStatus.h"
#include "OccurrenceKind.h"
#include "base/ptr.h"
#include "link_status_map_type.h"
#include <set>
#include <map>
#include <vector>

namespace sa {
  class Project;
  class Section;
  class Occurrence;
  class GlobalSymbol;
  class File;

  class Linker: public Process {
  public:
    Project *const project;
    
    Linker(Project *project);

    ~Linker();

    // Set the command used for linking. This allows the linker to take into
    // account the flags used for the actual link command. Initial value is an
    // empty list. Call while project is locked.
    //
    // TODO: generate a warning or an error when the link command is not set.
    // Do not run the linker in that case.  Filetree.mk will be invalid, so if
    // it is used, generated an error; otherwise, a warning is enough.
    
    void set_link_command(std::vector<std::string> command);

    // Update list of "forced" files, i.e. files in include mode.  Linker must
    // be locked for these calls. Calls do not automatically trigger the linker;
    // that is the caller's job.
    void _add_forced_file(File *file);
    void _remove_forced_file(File *file);

    // Remove section from linker
    void drop_section(Section *section);

    // Reload file if it is a linker script. Lock project while calling. Returns
    // immediately, reloading may happen later.
    void reload_as_linker_script(File *file);

  protected:

    // Link the program, derive file inclusion, detect undefined and multiply
    // defined globals and update the project.  Redefinition of Process::run().
    void run() override;

    // The core of the linking algorithm.  Repeatedly take a symbol in the todo
    // list and resolve it until no more symbols can be resolved. Leave any
    // unresolved symbols in the todo list.
    //
    // Any multiply-defined globals will set the linker status to error.
    // Unresolved globals will not set the linker status; it is up to do the
    // caller to either find another way to resolve them, or report an error.
    //
    // If an archive is given, resolve only using definitions in that archive.
    // Otherwise, resolve using definitions in any force-included or automatic
    // file.
    //
    void link(File *archive = 0);

    // Add a symbol to the todo list if has not been added yet. If the symbol
    // cannot be resolved, an error will be generated.
    void require(const std::string &name);

    // Add a symbol to the todo list if has not been added yet. If the symbol
    // cannot be resolved and is weakly required only, it will default to zero,
    // and no error will be generated.
    void weakly_require(const std::string &name);

    // Require all definitions of a given section.
    void require_section_defs(sa::Section *section);
    
    // If the given section is not yet in the list, add it and add all
    // of its linker relevant symbol uses to the todo list.
    //
    // The given definition is the reason why the section is included.
    // It is null if the section is forced.
    void add_section(Section *section, Occurrence *def = 0);

    // Add a definition.  If the definition's symbol was not defined yet, it
    // becomes defined; if it was already defined, it becomes multiply-defined.
    void add_definition(Occurrence *def);

    // Add a tentative definition.  If the definition's symbol was not
    // tentatively defined yet, it becomes tentatively defined.
    void add_tentative_definition(Occurrence *def);

    // Add a weak definition.  If the definition's symbol was not weakly defined
    // yet, it becomes weakly defined.
    void add_weak_definition(Occurrence *def);

    // If an archive is given, return true iff the file is that archive.
    // If no archive is given, return true for any force-included or
    // automatic file that is not an archive.
    bool allow_file(File *file, File *archive);
    
    // Add all sections instantiating the given definition.  This is usually one
    // section, but could be more if the definition is in an included file.  If
    // an archive is given, add only sections from that archive.  Otherwise, add
    // sections from any in any force-included or automatic file.
    void add_sections(base::ptr<Occurrence> definition, File *archive);

    // Checked version of add_sections. Only add sections after checking that
    // they actually instantiate the definition. Visit files at most once.  If
    // an archive is given, add only sections from that archive.  Otherwise, add
    // sections from any in any force-included or automatic file.
    void check_and_add_sections(
      base::ptr<Occurrence> definition,
      base::ptr<File> file,
      std::set<base::ptr<File>> &visited_files,
      File *archive
    );

    // Add sections of file instantiating definition.
    void add_sections_instantiating(
      base::ptr<Occurrence> definition,
      base::ptr<File> file
    );

    // Resolve global symbol by including all occurrences of the specified kind.
    // Kind should be a definition kind.  If an archive is given, resolve only
    // using definitions in that archive.  Otherwise, resolve using definitions
    // in any force-included or automatic file.
    void resolve_global(
      const std::string &name, OccurrenceKind okind, File *archive
    );

    // Report the current linker status.  This reports the change to the
    // application if changed.
    void report_linker_status();
    
    // Change linker and project status based on process status.
    void on_status(Status status) override;
    void on_out_of_date() override;

    void handle_stdout(std::istream &in);
    void handle_stderr(std::istream &in);

  private:
    // Initial linker status is error, because initially, there are no files, so
    // also no definition for the main function.
    LinkerStatus _linker_status = LinkerStatus_error;

    LinkerStatus _reported_linker_status = LinkerStatus_error;

    // Are sections garbage-collected? If so, the file's sections are only
    // included, and it's used symbols are only undefined, if at least one of
    // the symbols in the section is required.
    bool garbage_collect_sections = false;

    // Set of forced source files, i.e files with FileMode_include.
    std::set<base::ptr<File>> _forced_files;

    // List of command line source files, i.e. files added on the command line
    // or implicitly by the link command. This can include source files as well
    // as object files and archives.
    std::vector<base::ptr<File>> _command_files;

    // Linker script analyzer.
    base::ptr<LinkerScriptAnalyzer> _linker_script_analyzer = 0;

    // Implementation note: during linking, the project is locked, so no global
    // symbols will be destroyed.  This means that we can safely use a 'const
    // char*' to represent global symbol names, which avoids a lot of string
    // allocation, deallocation and copying during linking.

    // Names of linker-relevant symbols to be resolved.
    std::vector<std::string> todo;

    // Names of linker-relevant global symbols to be considered as resolved.
    // Includes both symbols that are actually resolved and symbols already on
    // the todo-list.
    std::set<std::string> resolved;

    // Names of symbols that are defined but possibly not yet resolved.
    std::vector<std::string> defined;

    // Set of sections to be linked.
    std::set<Section*> linked_sections;

    // Link status of global symbol names touched during linking.  Final link
    // status is copied to the symbols after linking.
    link_status_map_type status_map;

    // Diagnostics found during link command analysis;
    std::vector<base::ptr<Diagnostic>> link_command_diagnostics;

    // Symbols defined on the link command line
    std::set<std::string> defsyms;
    
  };
}

#endif
