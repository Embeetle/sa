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

#ifndef __LinkCommandAnalyzer_h
#define __LinkCommandAnalyzer_h

#include <string>
#include <vector>

// Analyze a link command, i.e. a program (such as `ld` or `gcc` or `g++`)
// together with its arguments used to link a program.  Figure out what extra
// files and libraries are linked, typically from the toolchain directory. Also
// figure out the default library search path, before any -L options added by
// the user.

namespace sa {
  class LinkCommandAnalyzer {
    
  public:
    // Diagnostic message when analysis fails, e.g. when no collect2 command is
    // found in the dry-run output. Empty on success.
    std::string diagnostic_message;

    // List of command arguments that are potentially files to be linked.
    // Original order is maintained because order matters for archives. Any -l
    // options are resolved, and those that cannot be resolved are added to the
    // unresolved_libs list instead.  Special values "(" and ")" represent
    // start-group and end-group respectively; archives in a group must be
    // searched repeatedly until no more symbols are resolved.
    std::vector<std::string> file_args;

    // List of unresolved -l options.
    std::vector<std::string> unresolved_libs;

    // Library search path, derived from the -L options as well as built-in
    // search directories of the linker command. First path should be tried
    // first.  Library search path is used for -l args as well as linker
    // scripts. All paths must be tried for all -l args and linker scripts, even
    // if the -L comes after the -l or script on the command line.
    std::vector<std::string> lib_search_path;

    // Args for script analysis, taken from user supplied args and implicit args
    // from the toolchain. Excludes source files, object files, libraries and
    // other args that are irrelevant for script analysis.
    std::vector<std::string> script_analysis_args;

    // Symbols defined on the command line using "--defsym name value" or
    // "--defsym=name=value". These should be treated as global symbol
    // definitions by the linker.
    std::vector<std::string> defsyms;

    // Are sections garbage-collected? If so, the file's sections are only
    // included, and it's used symbols are only undefined, if at least one of
    // the symbols in the section is required.
    bool garbage_collect_sections = false;

    // Directory in which the link command will be executed.
    std::string const build_path;

    // Create link command analyzer for given command, to be executed on the
    // given build path. Use the tmp_path folder for temporary files. tmp_path
    // must be an absolute path.
    LinkCommandAnalyzer(
      const std::vector<std::string> &command,
      const std::string &build_path,
      const std::string &tmp_path
    );

    // Add an entry to the library search path. Relative paths are made absolute
    // relative to the build path.
    //
    // Although this method is public, it should only be used internally.
    void add_lib_search_path(std::string const &path);

  protected:
    void handle_stdout(std::istream &in);
    void handle_stderr(std::istream &in);

  private:
    // To extract object files and archives added by the linker, we need a
    // temporary empty object file, and the path of that file needs to be passed
    // to the thread parsing the command output. This is the path of that file.
    std::string empty_o_path;

    // True when a collect2 command has been found and analyzed.
    bool analyzed = false;
  };
}

#endif
