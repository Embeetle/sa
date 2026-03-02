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

#ifndef __FlagExtractor_h
#define __FlagExtractor_h

#include "Process.h"
#include "Diagnostic.h"
#include "MakeCommandInfo.h"
#include "FlagInfo.h"
#include "base/filesystem.h"
#include <vector>
#include <set>

namespace sa {

  class Unit;
  class Linker;
  class File;
  class Hdir;

  // A make command analyzer is a process that extracts compiler and flags for
  // code analysis from a make command. It will also extract the command and
  // flags to be used for linking.
  //
  // Targets are source files for which flag extraction is required.  
  class FlagExtractor: public Process
  {
  private:
    // The make command.
    std::vector<std::string> make_command;
    std::string build_directory;

    // Pre-extracted make command info.  This info is only accessed from the
    // background thread, so no locking is needed.
    MakeCommandInfo make_command_info;

    // Flag indicating that make command info is out-of-date. This flag is set
    // from the foreground thread and read from the background thread, so the
    // flag extractor process must be locked when accessing it.
    bool make_command_info_out_of_date = false;

    // Targets for which flags are up to date. These targets are unblocked,
    // unless they are currently being analyzed; in that case, they will get
    // unblocked after analysis.
    std::vector<Unit*> old_targets;

    // Targets for which flags are not up to date yet. These targets are
    // blocked.
    std::vector<Unit*> new_targets;

    // Targets for which flag extraction is currently running. These targets are
    // blocked. This list can overlap with old and new targets.
    std::vector<Unit*> busy_targets;

    // The linker to which to pass linker flags extracted from the makefile.
    Linker *linker;

    // Time of last addition of a new target. To process as many targets as
    // possible with a single command, flag extraction is delayed for some time
    // after the last addition of a new target.
    double last_update = 0;

    // Flag info cache. Key is the file name, as returned by File::get_name() or
    // Project::get_normalized_path(path). Information is guaranteed to be
    // up-to-date when unit analyzer runs. A missing entry means that the make
    // command does not provide any compilation information about the file.
    std::map<std::string, base::ptr<FlagInfo>> flag_info_cache;

    // True if we have a link command. Reset for example when a makefile
    // changes.
    bool have_link_command = false;

    // Newly found link command in background thread. To be passed to linker
    // once it is complete.
    std::vector<std::string> link_command;

    // List of used makefiles
    std::vector<base::ptr<File>> makefiles;

    // Flag extraction diagnostics, e.g. makefile syntax errors.
    std::vector<base::ptr<Diagnostic>> new_diagnostics;
    std::vector<base::ptr<Diagnostic>> diagnostics;
    
  public:
    
    FlagExtractor(Linker *linker);
    ~FlagExtractor();

    // Set the make command and the directory in which to run it.
    void set_make_command(std::vector<std::string_view> command);

    // Set the build directory where the make command will be executed.
    void set_build_directory(std::string_view build_directory);

    // Add a new target.  Determine flags and analyze it. Non-source files are
    // ignored.
    void add_target(Unit *target);

    // Remove an existing target.
    void remove_target(Unit *target);

    // Update flags for a previously added target. This can be used for example
    // when hdirs have changed: instead of calling touch() to update flags for
    // all known targets, it it possible to determine which targets are affected
    // by the change, and update only those.
    void update_target(Unit *target);

    // If file is a makefile, re-analyze the make command to update flags for
    // all known targets.
    void reload_as_makefile(File *file);

    // Re-extract all flags
    void touch();

    // Postpone processing for a short while; more targets may be added soon.
    // This is useful when multiple targets can be analyzed at once, and the
    // analysis has a significant startup overhead.
    void wait();
    
  protected:
    void on_out_of_date() override;
    void on_up_to_date() override;
    void on_status(Status status) override;

    // Re-analyze everything.  Use when a makefile has been edited or the build
    // directory has changed. Assume that the process is locked.
    void _touch();

    // Clear cache for given target.
    void clear_cache(Unit *target);

    // Background thread entry point.  Update make command info, collect as many
    // files as possible to process in one go for efficiency, and process them.
    void run() override;

    
    // Get flag info for all busy units, assuming the given set of
    // application-added hdirs.  To be called from background thread.  Dry-run
    // make and analyze output.
    void process(std::set<std::string> const &hdir_paths);

    void handle_stdout(std::istream &in);
    void handle_stderr(std::istream &in);

    // Analyze flags for a single compiler command from the make dry-run,
    // and enter results in cache.
    //
    // Compiler is the compiler path as given in the command line.
    // 
    // Flags start from pos in command line.
    void analyze_compiler_args(
      std::string_view compiler,
      std::string_view command_line,
      size_t pos
    );

    // Normalize path read from the make dry-run: make it absolute wrt the build
    // directory and normalize it.
    std::string norm_path(std::string const &path);

    // Get path to a file from the build directory. If the file is nested in the
    // project directory, return a relative path to avoid issues with spaces in
    // the project path.  Otherwise, return the given absolute path.
    std::string get_build_path(std::string const &abs_path);

    void check_target_block_status(Unit *target);
    void _check_target_block_status(Unit *target);
  };
}

#endif

