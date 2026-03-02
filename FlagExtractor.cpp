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

#include "FlagExtractor.h"
#include "MakeCommandInfo.h"
#include "Unit.h"
#include "File.h"
#include "Hdir.h"
#include "Project.h"
#include "Linker.h"
#include "LineOffsetTable.h"
#include "compiler.h"
#include "environment.h"
#include "base/time_util.h"
#include "base/string_util.h"
#include "base/os.h"

sa::FlagExtractor::FlagExtractor(Linker *linker)
  : Process("@flag-extractor")
  , linker(linker)
{
  set_urgent(true);
  // Above call also does lock/unlock so release release initial values to other
  // threads; no need to do it again.
  //Lock lock(this);
}

sa::FlagExtractor::~FlagExtractor()
{
  assert_(is_up_to_date(), process_name());
  for (auto target: new_targets) {
    target->unblock();
  }
}

void sa::FlagExtractor::set_make_command(
  std::vector<std::string_view> command
)
{
  assert(linker->project->is_locked());
  trace("FlagExtractor set make command: "
    << base::quote_command_line(command)
  );
  std::vector<std::string> new_make_command
    = std::vector<std::string>(command.begin(), command.end());
  if (make_command != new_make_command) {
    make_command.swap(new_make_command);
    _touch();
  }
}

// Find an element in a vector.  Return true iff the element was found.
template <typename T>
static bool find_in_vector(std::vector<T> const &v, T e)
{
  return std::find(v.begin(), v.end(), e) != v.end();
}

// Remove an element from a vector.  Return true iff the element was found.
template <typename T>
static bool remove_from_vector(std::vector<T> &v, T e)
{
  auto index = std::find(v.begin(), v.end(), e);
  if (index == v.end()) {
    return false;
  }
  std::swap(*index, v.back());
  v.pop_back();
  return true;
}

void sa::FlagExtractor::add_target(Unit *target)
{
  assert(linker->project->is_locked());
  trace_nest("FlagExtractor add: " << target->process_name());
  assert(base::is_valid(target));
  assert(base::is_valid(target->file));
  Lock target_lock(target);
  assert(!find_in_vector(old_targets, target));
  assert(!find_in_vector(new_targets, target));
  //
  // Does this target require flag extraction? If so, block it until flag
  // extraction is done.  We try to do flag extraction for multiple files at
  // once if possible; see run method.
  //
  // Currently,  only the Clang and assembly analyzers support flags.
  if (target->file->is_source_file()) {
    trace_nest("FlagExtractor add unit=" << (void*)target
      << " " << target->process_name()
    );
    // Block target: it will be unblocked once its flags are known. Flag
    // extraction is invoked by the run method for all new targets.
    target->_block();

    // Trigger flag extraction for this target.
    new_targets.push_back(target);
    trace("add      " << target->_get_block_count() << " "
      << target->file->get_name()
      << " " << old_targets.size() << "/" << new_targets.size()
    );
    trace("new=" << new_targets.size() << " old=" << old_targets.size());
    wait();
  }
    
  // Make sure that the target is triggered at least once, even if flags keep
  // their initial value (=no flags).
  target->_trigger();
  _check_target_block_status(target);
}

void sa::FlagExtractor::remove_target(Unit *target)
{
  assert(linker->project->is_locked());
  Lock target_lock(target);
  trace_nest("FlagExtractor remove: " << target->file->get_name());
  if (target->file->is_source_file()) {
    trace_nest("FlagExtractor remove unit=" << (void*)target
      << " " << target->process_name()
    );
    if (remove_from_vector(new_targets, target)) {
      trace("was new target");
      target->_unblock();
      clear_cache(target);
    } else {
      bool removed = remove_from_vector(old_targets, target);
      assert(removed);
      trace("was old target");
    }
    if (remove_from_vector(busy_targets, target)) {
      trace("removed busy target " << target->file->get_name() << ", " <<
        busy_targets.size() << " remaining"
      );
      target->_unblock();
    }      
    check_assert(!find_in_vector(new_targets, target));
    check_assert(!find_in_vector(old_targets, target));
    check_assert(!find_in_vector(busy_targets, target));
    assert(!target->_is_blocked());
    // Re-trigger. This is essential if the flag extraction process is already
    // running: we want to make sure that it doesn't access the removed and
    // possibly deleted unit.
    trigger();
    trace("remove   " 
      << target->_get_block_count() << " " << target->file->get_name()
      << " " << old_targets.size() << "/" << new_targets.size()
    );
    trace("new=" << new_targets.size() << " old=" << old_targets.size());
  }
  _check_target_block_status(target);
}

void sa::FlagExtractor::update_target(Unit *target)
{
  assert(linker->project->is_locked());
  trace_nest("FlagExtractor update: " << target->process_name());
  assert(base::is_valid(target));
  assert(base::is_valid(target->file));

  Lock target_lock(target);
  //
  // Does this target require flag extraction? If so, block it until flag
  // extraction is done.  We try to do flag extraction for multiple files at
  // once if possible; see run method.
  //
  // Currently,  only the Clang and assembly analyzers support flags.
  if (target->file->is_source_file()) {
    trace_nest("FlagExtractor update unit=" << (void*)target
      << " " << target->process_name()
    );
    if (remove_from_vector(old_targets, target)) {
      trace("was old target");
      // Block target: it will be unblocked once its flags are known. Flag
      // extraction is invoked by the run method for all new targets.
      target->_block();

      // Trigger flag extraction for this target.
      new_targets.push_back(target);
      trace("update " 
        << target->_get_block_count() << " " << target->file->get_name()
        << " " << old_targets.size() << "/" << new_targets.size()
      );
      wait();
    } else {
      trace("was new target");
      trace("re-update " 
        << target->_get_block_count() << " " << target->file->get_name()
        << " " << old_targets.size() << "/" << new_targets.size()
      );
    }
    check_assert(find_in_vector(new_targets, target));
    check_assert(!find_in_vector(old_targets, target));
    assert(target->_is_blocked());
    trace("new=" << new_targets.size() << " old=" << old_targets.size());
  }
  _check_target_block_status(target);
}

void sa::FlagExtractor::reload_as_makefile(File *file)
{
  assert(linker->project->is_locked());
  trace_nest("Flag extractor: reload as makefile: " << file->get_name());
  for (std::string const &path: make_command_info.makefiles()) {
    if (path == file->get_path()) {
      _touch();
    }
  }
}

void sa::FlagExtractor::set_build_directory(std::string_view build_directory)
{
  assert(linker->project->is_locked());
  trace_nest("Flag extractor: update build directory");
  this->build_directory = build_directory;
  _touch();
}

void sa::FlagExtractor::touch()
{
  assert(linker->project->is_locked());
  _touch();
}
  
void sa::FlagExtractor::_touch()
{
  trace_nest("FlagExtractor touch with " << old_targets.size()
    << " old targets and " << new_targets.size() << " new targets"
  );
  if (!old_targets.empty()) {
    for (auto target: old_targets) {
      trace("new target: " << target->file->get_name());
      target->block();
      new_targets.push_back(target);
      trace("touch " << target->get_block_count() << " "
        << target->file->get_name()
      );
      check_target_block_status(target);
    }
    old_targets.clear();
  }
  make_command_info_out_of_date = true;
  have_link_command = false;
  wait();
}

void sa::FlagExtractor::wait()
{
  assert(linker->project->is_locked());
  last_update = base::get_time();
  trigger();
}

void sa::FlagExtractor::clear_cache(Unit *unit)
{
  flag_info_cache.erase(unit->file->get_key());
}

void sa::FlagExtractor::on_out_of_date()
{
  trace_nest("FlagExtractor out-of-date");
  linker->block();
}

void sa::FlagExtractor::on_up_to_date()
{
  trace_nest("FlagExtractor up-to-date");
  // Project is not locked at this point, and I cannot lock it because I am
  // already locked (risk of deadlock), so no way currently to report the fact
  // that I am now up to date.
  linker->unblock();
}

void sa::FlagExtractor::on_status(Status status)
{
  trace(process_name() << " status: " << status);
}

void sa::FlagExtractor::run()
{
  trace_nest("FlagExtractor run");
  // Cache data shared with foreground.
  double enter_time = base::get_time();
  double wait_after;
  bool update_make_command_info;
  std::vector<std::string> local_make_command;
  std::string local_build_directory;
  {
    Project::Lock project_lock(linker->project);
    wait_after = last_update;
    update_make_command_info = make_command_info_out_of_date;
    make_command_info_out_of_date = false;
    local_make_command = make_command;
    local_build_directory = build_directory;
  }
  if (update_make_command_info) {
    trace_nest("Extract make command info");
    make_command_info.set_make_command(
      std::vector<std::string_view>(
        local_make_command.begin(), local_make_command.end()
      ), local_build_directory
    );
    {
      Project::Lock project_lock(linker->project);
      std::vector<base::ptr<File>> alt_makefiles;
      std::vector<std::string> const &makefile_paths
        = make_command_info.makefiles();
      for (auto const &path: makefile_paths) {
        base::ptr<File> file = linker->project->get_file(path);
        alt_makefiles.emplace_back(file);
        file->inc_inclusion_count("used as makefile");
      }
      makefiles.swap(alt_makefiles);
      for (auto &file: alt_makefiles) {
        file->dec_inclusion_count("used as makefile");
      }
    }
    // At this point, the make command info might be out-of-date again, but
    // don't worry: in that case, this process has already been cancelled and
    // will be restarted.
  }

  // Postpone update until there are no new requests for updates for at least
  // flag_extraction_delay seconds.
  double const flag_extraction_delay = 0.500;
  trace("Flag extractor: wait for more targets last-update="
    << wait_after << " so wait until "
    << (wait_after + flag_extraction_delay)
  );
  base::sleep_until(wait_after + flag_extraction_delay);
  double start_time = base::get_time();

  // Take a copy of the new targets list while the process is locked, and then
  // unlock the process.
  //
  // It doesn't matter that the process was temporarily unlocked above: the only
  // risk is that new targets were added, which is actually good, not bad.
  //
  // We cannot keep the process locked after taking a copy of the list, because
  // flag extraction takes time, and because flag extraction will lock the
  // project, and our convention to avoid deadlocks is that if both the process
  // and the project are locked, then the project needs to be locked
  // first. Also, unlocking the process allows new targets to be added while
  // flags are extracted for old targets.
  //
  // Since we cannot lock this process nor the project while processing the
  // targets, we need to make sure that the targets are not accessed when
  // removed and potentially deleted. Do ensure this, removing a target
  // re-triggers the flag extraction process, and the flag extraction process
  // always locks itself and checks for cancellation before accessing the list
  // of targets.
  //
  std::set<std::string> hdir_paths;
  std::string const project_path;
  size_t nr_targets = 0;
  {
    Project::Lock project_lock(linker->project);
    if (cancelled()) {
      trace("Flag extractor: cancelled due to change, restart");
      return;
    }
    nr_targets = new_targets.size();
    trace_nest("get " << nr_targets << " targets");
    // Continue even if there are no new targets, to extract linker command
    //if (new_targets.empty()) return;
    assert(busy_targets.empty());
    busy_targets.swap(new_targets);
    for (auto target: busy_targets) {
      assert(base::is_valid(target));
      check_target_block_status(target);
      target->set_analysis_status(
        AnalysisStatus_waiting, "flag update requested"
      );
      clear_cache(target);
      old_targets.emplace_back(target);
      trace(
        "extract " << target->get_block_count() << " " << target->process_name()
      );
    }
    {
      // Copy hdir set to hdir path set to sort based on path iso pointer.
      trace("FlagExtractor: " << linker->project->added_hdirs().size()
        << " hdirs"
      );
      for (auto const &hdir: linker->project->added_hdirs()) {
        hdir_paths.insert(hdir->path);
      }
      trace_nest("Hdir paths: " << hdir_paths.size());
      for (auto const &hdir_path: hdir_paths) {
        trace(hdir_path << " => " << get_build_path(hdir_path));
      }
    }
  }

  // Actually extract flags.
  //
  // It is OK to enter results in the cache even when the process was cancelled:
  // it is the responsibility of the canceller to move the invalidated targets
  // back to the old_targets list, so they will be re-processed.
  //
  // Feel free to use extracted make command info without lock.
  //
  process(hdir_paths);

  trace("processed");
  {
    Project::Lock project_lock(linker->project);
    trace_nest("update targets");
    trace("finalize " << busy_targets.size() << " busy targets");
    // Continue loop, whether cancelled or not: all targets we removed from the
    // new_targets list need to be unblocked. If they have been re-triggered in
    // the mean time, they are twice blocked at this point!
    // Take a copy of the list of busy targets or iterate backwards, because the
    // list can change while iterating due to removed targets.
    for (size_t i = busy_targets.size(); i--; ) {
      Unit *target = busy_targets[i];
      check_target_block_status(target);
      base::ptr<File> file = target->file;
      auto it = flag_info_cache.find(file->get_key());
      trace("upd&unbl " << target->get_block_count() << " " << file->get_name());
      assert(find_in_vector(busy_targets, target));
      if (it != flag_info_cache.end()) {
        trace(" `-> use cached data");
        target->update_analysis_flags_and_unblock(it->second);
      } else {
        trace(" `-> no cached data, " << new_diagnostics.size()
          << " make-diagnostics"
        );
        std::vector<base::ptr<Diagnostic>> target_diagnostics;
        // If flag extraction failed, e.g. due to a syntax error in the
        // makefile, a fatal error has already been reported, so we don't need
        // to report an additional error for each source file.
        if (new_diagnostics.empty()) {
          trace("add target diagnostic");
          target_diagnostics.emplace_back(
            linker->project->get_diagnostic(
              "Don't know how to compile " + file->get_name(),
              Severity_fatal, Category_makefile
            )
          );
        }
        FlagInfo flag_info(target_diagnostics);
        target->update_analysis_flags_and_unblock(&flag_info);
      }
    }
    busy_targets.clear();
    //
    // Report flag extraction errors,  e.g makefile syntax errors.
    linker->project->add_unit_diagnostics(new_diagnostics);
    new_diagnostics.swap(diagnostics);
    linker->project->remove_unit_diagnostics(new_diagnostics);
    new_diagnostics.clear();
  }

  double end_time = base::get_time();
  debug_atomic_writeln(
    std::fixed << std::setw(10) << std::setprecision(6)
    << "flag extractor lead time = " << (start_time-enter_time)
    << " run time = "  << (end_time-start_time)
    << " for " << nr_targets << " targets"
  );
}

void sa::FlagExtractor::process(
  std::set<std::string> const &hdir_paths
)
{
  trace_nest("process " << busy_targets.size() << " targets");
  // Write the targets to a temporary makefile.  Do not add them to the command
  // line, because especially on Windows, the command line can easily become too
  // long.
  std::string temp_makefile_path;
  {
    std::ofstream temp_makefile;
    temp_makefile_path = base::open_temp_for_writing(
      temp_makefile, "embeetle_dry_run_", ".mk"
    );
    trace("temp file: " << temp_makefile_path);
    if (!temp_makefile.is_open()) {
      std::vector<base::ptr<Diagnostic>> diagnostics;
      Project::Lock project_lock(linker->project);
      diagnostics.emplace_back(
        linker->project->get_diagnostic(
          "Cannot open temporary file for makefile analysis",
          Severity_fatal, Category_makefile
        )
      );
      base::ptr<FlagInfo> info = base::ptr<FlagInfo>::create(diagnostics);
      // Lock flag extractor while accessing units and do not access units when
      // cancelled: some of those units might be removed and deleted (=dangling
      // pointer)!
      if (!cancelled()) {
        for (auto target: busy_targets) {
          flag_info_cache[target->file->get_key()] = info;
        }
      }
      return;
    }
    temp_makefile << "embeetle_dry_run: \\\n"
                  << "  " << make_command_info.default_goal() << " \\\n";
    {
      // Lock flag extractor while accessing units and do not access units when
      // cancelled: some of those units might be removed and deleted (=dangling
      // pointer)!
      Project::Lock project_lock(linker->project);
      if (!cancelled()) {
        // Add all named goals?  That would increase the chances of finding the
        // intended link command, and also allow us to detect the presence of
        // multiple link commands, all in a single run of `make`. TODO
        for (auto target: busy_targets) {
          base::ptr<File> file = target->file;
          std::string source_path = get_build_path(file->get_path());
          trace("relative path of " << file->get_name()
            << " in " << make_command_info.build_directory()
            << " is " << source_path
          );
          std::string target_path
            = make_command_info.choose_target(source_path);
          trace("for '" << source_path << "' build '" << target_path << "'");
          temp_makefile << "  " << target_path << " \\\n";
        }
      }
    }
    temp_makefile << "\n";
    //
    // Write Makefile assignments to override HDIR_FLAGS and HDIRS.
    //
    // The HDIR_FLAGS variable defined in filetree.mk (if included from the
    // makefile) must be overridden.  This is required because the HDIR_FLAGS
    // in filetree.mk contains directories that are marked as used by the
    // source analyzer; this information is unavailable during Clang
    // analysis. It is also required because analysis needs to try all
    // potential hdirs, including those that are in automatic mode and
    // previously classified as unused by the source analyzer.
    //
    // Paths in HDIR_FLAGS must be relative to the build directory or
    // absolute, and spaces or other special characters must be quoted for
    // the command shell.
    //
    // For compatibility with pre-v6 makefiles, we also define HDIRS.  Paths
    // in HDIRS must be relative to the source directory, because the
    // makefile will prefix them with the path of the source directory
    // relative to the build directory. There is no need for shell quoting,
    // because pre-v6 makefiles could not handle spaces or special characters
    // in include paths.
    //
    // It is not a good idea to override these variables on the command
    // line. Some projects - notably our first HPMicro project - have so many
    // hdirs that the command line becomes too long for Windows.
    //
    // The hdirs are sorted alphabetically, for reproducability. If a header
    // with the same name exists in multiple directories, the hdir order may
    // affect the result. Note: hdirs are stored in a set, which is
    // automatically sorted.
    //
    temp_makefile << "override HDIR_FLAGS=";
    for (auto const &hdir_path: hdir_paths) {
      temp_makefile << " -I" << base::quote_command_arg(
        get_build_path(hdir_path)
      );
    }
    temp_makefile << "\n";
    temp_makefile << "override HDIRS=";
    for (auto const &hdir_path: hdir_paths) {
      temp_makefile << " " << base::get_relative_path(
        hdir_path, linker->project->get_project_path()
      );
    }
    temp_makefile << "\n";
  }
  //
  // The compilation commands are extracted by running `make` with the
  // `--dry-run` option and capturing the output.
  //
  std::vector<const char*> args;
  for (std::string const &arg: make_command) {
    args.emplace_back(arg.data());
  }
  //
  // The `--dry-run` option tells `make` to print the commands to be
  // executed instead of actually executing them.
  //
  args.emplace_back("--dry-run");
  //
  // The `--no-print-directory` option tells `make` to not print the
  // directory in which it executes. We may need to remove that in the
  // future in order to handle recursive makefiles.
  //
  args.emplace_back("--no-print-directory");
  //
  // The `--always-make` option tells make to rebuild the target
  // unconditionally, even if it seems up-to-date. In combination with
  // --dry-run, this will unconditionally print the commands to be executed
  // to rebuild the target.
  //
  args.emplace_back("--always-make");
  //
  // The `--keep-going` option tells make to keep going in case of errors.
  // If some of the source paths do not have compilation commands in the
  // makefile, this allows us to still extract the compilation commands for
  // the other source files.
  //
  args.emplace_back("--keep-going");
  //
  // Set OFILES and PROJECT_LDLIBS to empty value.  Extractor results should be
  // independent of OFILES and PROJECT_LDLIBS as defined in filetree.mk, because
  // the flag extractor might run before filetree.mk is created/updated.  Also,
  // the link command should not include any of the object files added via
  // OFILES or libraries added via PROJECT_LDLIBS. Object files and libraries in
  // the link command will be analyzed as binary files for global symbols, and
  // the files in OFILES and PROJECT_LDLIBS should not be analyzed that way,
  // because their corresponding source files will already be analyzed, and
  // because they might not exist yet.
  //
  // TODO: this solution is not ideal, as it will also affect non-Embeetle
  // makefiles that happen to use OFILES and/or PROJECT_LDLIBS. Better would be
  // to add a mechanism to skip the inclusion of filetree.mk based on a setting
  // with EMBEETLE in its name; it is improbable that an arbitrary makefile will
  // contain such a variable. TODO
  //
  // TODO: object files added to the link command using any mechanism other than
  // OFILES that can be rebuild from source files by make should also be
  // filtered out, and the corresponding source file should be added as included
  // so that it can be analyzed.  This is mainly relevant for arbitrary
  // makefiles, not those in downloaded Embeetle projects.  Note:
  // self._makefile_rules contains a list of (target, dep) pairs that can be
  // used to check whether an object file can be rebuild.
  //
  args.emplace_back("OFILES=");
  args.emplace_back("PROJECT_LDLIBS=");
  //
  // Include the temp files generated higher
  //
  args.emplace_back("-f");
  args.emplace_back(temp_makefile_path.data());
  args.emplace_back("embeetle_dry_run");
  debug_atomic_writeln("make dry-run command: cd "
    << base::quote_command_arg(make_command_info.build_directory())
    << " && " << base::quote_command_line(args)
  );
  args.push_back(0);
  int exit_code;
  int error_code = base::os::execute_and_capture(
    args.data(),
    make_command_info.build_directory().data(),
    this,
    &FlagExtractor::handle_stdout,
    &FlagExtractor::handle_stderr,
    exit_code, standard_environment
  );
  trace("error_code=" << error_code << " exit_code=" << exit_code);
  
  debug_atomic_writeln("keep " << temp_makefile_path << " for debugging");
  no_debug_code(base::remove(temp_makefile_path));
}

void sa::FlagExtractor::handle_stdout(std::istream &in)
{
  trace_nest("FlagExtractor handle stdout");
  std::string line;
  std::string word;  // keep outside loop for efficiency!
  for (;;) {
    base::getline(in, line);
    if (!in) break;
    trace("OUT: " << line);

    // Look for a word matching the compiler regex.  This is not necessarily
    // the first word of a command line.  Some tools, like 'purify' or
    // 'gprof', insert another command before the actual compiler command.
    // We will look for the first word that matches, and assume that all
    // following words are compiler flags.
    //
    // TODO: filter out shell metacharacters such as '|' and '>' that
    // might occur in commands.
    size_t pos = 0;
    for (;;) {
      pos = base::unquote_command_word(word, line, pos);
      if (pos == word.npos) break;
      std::string_view cmd = base::os::strip_exe_extension(word);
      if ( (cmd.size() == 3 || (cmd.size() > 3 && cmd[cmd.size()-4] == '-'))
        && (cmd.substr(cmd.size()-3) == "gcc"
          || cmd.substr(cmd.size()-3) == "g++"
        )
      ) {
        trace("  compiler: " << cmd);
        analyze_compiler_args(cmd, line, pos);
        break;
      }
    }
  }
}

void sa::FlagExtractor::handle_stderr(std::istream &in)
{
  trace_nest("FlagExtractor handle stderr");
  // Make sure to interpret output even in the presence of errors in the error
  // output.  Error output may be caused by some targets (e.g. the link goal),
  // while others do yield usable information (e.g. object files).
  std::string line;
  for (;;) {
    base::getline(in, line);
    if (!in) break;
    trace("ERR: " << line);
    // Examples:
    // ../config/makefile:80: *** missing separator.  Stop.
    // ../config/makefile.d/simulate.mk:6: *** missing separator.  Stop.
    // make[1]: warning: jobserver unavailable: using -j1.  (continued)
    //    Add '+' to parent make rule.
    // make[1]: *** No rule to make target 'foofoo.o', needed by '...'.
    size_t pos = 0;
    if (base::begins_with(line, "make[")) {
      trace("skip make[...]: ");
      pos = line.find(' ', 6);
      if (pos == std::string::npos) {
        trace("no space found");
        continue;
      }
      ++pos;
    }
    if (base::substr_is(line, pos, "warning: ")) {
      trace("ignore warning");
      continue;
    }
    if (base::substr_is(line, pos, "*** No rule to make target '")) {
      trace("ignore no rule to make target");
      continue;
    }
    size_t colon1 = line.find(':', pos);
    if (colon1 != std::string::npos) {
      std::string raw_path(line, pos, colon1);
      trace("raw path: " << raw_path);
      Project *project = linker->project;
      Project::Lock lock(project);
      std::string path = base::get_absolute_path(
        raw_path, project->get_build_path()
      );
      trace("path: " << path << " (" << base::is_file(path) << ")");
      if (base::is_file(path)) {
        base::ptr<File> file = project->get_file(path);
        unsigned offset = 0;
        size_t message_offset = colon1+1;
        size_t colon2 = line.find(':', colon1+1);
        if (colon2 != std::string::npos) {
          message_offset = colon2+1;
          const char *begin = line.data()+colon1+1;
          char *end = 0;
          long line_number = strtol(begin, &end, 10);
          if (end > begin && line_number > 0) {
            std::ifstream stream(file->get_path().data());
            if (stream) {
              LineOffsetTable table(stream);
              offset = table.offset_or_zero(line_number-1, 0);
            }
          }
        }
        size_t message_begin = line.find_first_not_of(" \t\v", message_offset);
        if (message_begin != std::string::npos) {
          std::string message(line, message_begin);
          base::remove_suffix(message, "  Stop.");
          auto diagnostic = file->get_diagnostic(message,Severity_fatal,offset);
          new_diagnostics.push_back(diagnostic);
        }
      }
    }
  }
}

void sa::FlagExtractor::analyze_compiler_args(
  std::string_view compiler,
  std::string_view command_line,
  size_t pos
)
{
  trace_nest("analyze compiler args: " << command_line.substr(pos));
  trace("build directory: " << make_command_info.build_directory());
  std::vector<std::string> source_paths;
  std::vector<std::string> analysis_flags;
  std::vector<std::string> compiler_flags;
  //
  // We only accept the first link command. Extra link commands should probably
  // result in a diagnostic.
  bool can_be_link_command = !have_link_command;
  if (can_be_link_command) {
    link_command.clear();
    link_command.push_back(std::string(compiler));
  }
  //
  bool next_is_path = false;
  bool next_is_arg = false;
  bool next_is_ignored = false;
  std::string parameter;
  for (;;) {
    pos = base::unquote_command_word(parameter, command_line, pos);
    if (pos == command_line.npos) break;
    if (can_be_link_command) {
      link_command.push_back(parameter);
    }
    trace("command parameter: " << parameter << " path=" << next_is_path
      << " arg=" << next_is_arg << " ignored=" << next_is_ignored
    );
    if (next_is_path) {
      next_is_path = false;
      if (next_is_ignored) {
        next_is_ignored = false;
      } else {
        analysis_flags.emplace_back(norm_path(parameter));
      }
    } else if (next_is_arg) {
      next_is_arg = false;
      if (next_is_ignored) {
        next_is_ignored = false;
      } else {
        analysis_flags.emplace_back(parameter);
      }
    } else if (parameter[0] != '-') {
      // This parameter is not a flag, and was not preceded by a flag that needs
      // another parameter, so it must be a source file.
      //
      // Source paths in the compilation command are either absolute or
      // relative to the build directory.  Get the corresponding absolute
      // normalized path, so that it can be matched to source paths for which
      // flags are requested.
      //
      // There is no need to add it to any flags list; source files will be
      // implicitly added by analysis code.
      source_paths.emplace_back(norm_path(parameter));
    } else {
      // This parameter is a flag.  It may or may not contain a path, be
      // followed by a path or be followed by a non-path argument. It may or may
      // not be relevant for source analysis.
      if (sa::is_compiler_builtin_relevant(parameter)) {
        // This flag is relevant for extraction of built-in compiler flags; it
        // may or may not be ignored as user flag.
        compiler_flags.push_back(parameter);
      }
      // No need to check leading dash again, skip it
      const char *flag1 = parameter.data()+1;
      // Does this flag suppress linking? 
      if (can_be_link_command) {
        const char *nonlink_flags[] = { "-c", "-S", "-E" };
        for (auto nonlink_flag: nonlink_flags) {
          if (!strcmp(flag1, nonlink_flag+1)) {
            can_be_link_command = false;
            link_command.clear();
            break;
          }
        }
      }
      // Should this flag be ignored for source analysis?
      //
      // Some compiler flags should not be passed to Clang, either because they
      // cause undesired side-effects or to avoid the overhead.
      //
      //  -M... flags that tell the compiler to generate dependency files.
      //  -o specifies where the output should go
      //  -m... flags are target specific and control code generation,
      //        with some exceptions
      //  -v and -### generate verbose output
      //  --save-temps saves temporary files
      //  -EB and -EL select big-endian or little-endian for some machines
      //
      // etc. For an overview of options, see for example
      // https://gcc.gnu.org/onlinedocs/gcc/Option-Summary.html
      //
      // Getting this regex right is hard and requires gradual refinement.  Here
      // are some options that should be passed to Clang, because they have an
      // effect on errors and warnings:
      //
      //  -fpermissive
      //  -pedantic         strict ISO standard checking
      //  -pedantic-errors
      //  -w                inhibits all warning messages 
      //  -W...             turns warnings into errors or vice versa,
      //                    or suppresses warnings etc
      //  -fmax-errors=n    limits the maximum number of errors emitted
      //  -fsyntax-only     checks for syntax errors only
      //
      // Options controlling the C dialect:
      //
      //  -ansi  -std=standard  -fgnu89-inline 
      //  -fpermitted-flt-eval-methods=standard 
      //  -aux-info filename  -fallow-parameterless-variadic-functions 
      //  -fno-asm  -fno-builtin  -fno-builtin-function  -fgimple
      //  -fhosted  -ffreestanding 
      //  -fopenacc  -fopenacc-dim=geom  -fopenacc-kernels=mode 
      //  -fopenmp  -fopenmp-simd 
      //  -fms-extensions  -fplan9-extensions  -fsso-struct=endianness 
      //  -fallow-single-precision  -fcond-mismatch  -flax-vector-conversions 
      //  -fsigned-bitfields  -fsigned-char 
      //  -funsigned-bitfields  -funsigned-char
      //
      // Options controlling the C++ dialect:
      //
      //  -fabi-version=n  -fno-access-control 
      //  -faligned-new=n  -fargs-in-order=n  -fchar8_t  -fcheck-new 
      //  -fconstexpr-depth=n  -fconstexpr-cache-depth=n 
      //  -fconstexpr-loop-limit=n  -fconstexpr-ops-limit=n 
      //  -fno-elide-constructors 
      //  -fno-enforce-eh-specs 
      //  -fno-gnu-keywords 
      //  -fno-implicit-templates 
      //  -fno-implicit-inline-templates 
      //  -fno-implement-inlines  -fms-extensions 
      //  -fnew-inheriting-ctors 
      //  -fnew-ttp-matching 
      //  -fno-nonansi-builtins  -fnothrow-opt  -fno-operator-names 
      //  -fno-optional-diags  -fpermissive 
      //  -fno-pretty-templates 
      //  -fno-rtti  -fsized-deallocation 
      //  -ftemplate-backtrace-limit=n 
      //  -ftemplate-depth=n 
      //  -fno-threadsafe-statics  -fuse-cxa-atexit 
      //  -fno-weak  -nostdinc++ 
      //  -fvisibility-inlines-hidden 
      //  -fvisibility-ms-compat 
      //  -fext-numeric-literals
      //
      // Some flags match the ignore rules,  but should not be ignored.
      //
      //  -mfloat-abi=... affects the builtin __SOFTFP__ macro in clang as well
      //                  as gcc, in the same way.  By default, __SOFTFP__ is
      //                  defined.  -mfloat-abi=hard undefines __SOFTFP__.
      //
      bool ignored = (
        strchr("Mom", flag1[0]) // followed by anything
        || ( strchr("cvESuLT", flag1[0]) && !flag1[1])
        // Improvement vs python code:
        //|| (flag1[0] == 'E' && (flag1[1] == 'B' || flag1[1] == 'E'))
        || !strcmp(flag1, "-###"+1)
        || !strcmp(flag1, "--param"+1)
        || !strcmp(flag1, "-pipe"+1)
        || !strcmp(flag1, "-wrapper"+1)
        || !strcmp(flag1, "--version"+1)
        || !strcmp(flag1, "--target-help"+1)
        || !strncmp(flag1, "--help"+1, 5)
        || !strcmp(flag1, "-dumpdir"+1)
        || !strncmp(flag1, "-dumpbase"+1, 8)
        || !strncmp(flag1, "-ffile-prefix-map"+1, 16)
        || !strncmp(flag1, "-fplugin"+1, 7)
        || !strncmp(flag1, "-fdump"+1, 5)
        || !strcmp(flag1, "-fada-spec-parent"+1)
        || !strcmp(flag1, "-fno-fat-lto-objects"+1)
      ) && (
        strncmp(flag1, "-mfloat-abi="+1, 11)
      );
      // Does this flag precede a path?
      //
      // Any relative paths must be made absolute for Clang, since we cannot
      // actually change directory to the build directory.
      //
      // The path can be in the flag itself (-Ifoo) or in the next parameter
      // (-I foo). Handle both cases.
      //
      const char *path_flags[] = {
        "-o", "-L", "-I", "-isystem", "-include", "-imacros", "-idirafter",
        "-iprefix", "-MF", "-T"
      };
      const char *path_flag_core = 0;
      const char *path_flag_path = 0;
      for (auto path_flag: path_flags) {
        size_t path_pos = base::common_prefix_length(path_flag+1, flag1);
        if (!path_flag[path_pos+1]) {
          path_flag_core = path_flag;
          path_flag_path = flag1 + path_pos;
          break;
        }
      }
      trace(" `-> process " << parameter << " ignored=" << ignored
        << " path-flag=" << (path_flag_path ? path_flag_core : "<none>")
      );
      if (path_flag_core) {
        if (*path_flag_path) {
          // This is a parameter followed by a path in the parameter itself,
          // like -I/foo/bar
          if (!ignored) {
            analysis_flags.emplace_back(
              path_flag_core + norm_path(path_flag_path)
            );
          }
        } else {
          // This is a parameter followed by a path in the next parameter, like
          // -I /foo/bar
          next_is_path = true;
          next_is_ignored = ignored;
          if (!ignored) {
            analysis_flags.emplace_back(parameter);
          }
        }
      } else {
        // Non-path parameter. Some flags are followed by a non-path argument.
        // In that case, the next item should not be interpreted on its own
        // (e.g. as source file) but either simply copied to Clang or ignored.
        const char *arg_flags[] = { "-u", "-MT", "-MQ", "-A", "-x", "--param" };
        next_is_arg = *flag1 == 'X';
        if (!next_is_arg) {
          for (auto arg_flag: arg_flags) {
            if (!strcmp(flag1, arg_flag+1)) {
              next_is_arg = true;
              break;
            }
          }
        }
        if (next_is_arg) {
          next_is_ignored = ignored;
        }
        if (!ignored) {
          analysis_flags.emplace_back(parameter);
        }
      }
    }
  }
  trace("analysis flags: " << analysis_flags.size());
  trace("compiler flags: " << compiler_flags.size());
  trace("source paths: " << source_paths);
  trace("compiler: " << compiler);
  trace("can be link command: " << can_be_link_command);

  base::ptr<FlagInfo> info = base::ptr<FlagInfo>::create(
    std::string(compiler), compiler_flags, analysis_flags
  );
  {
    Project *project = linker->project;
    Project::Lock project_lock(project);
    for (auto const &source_path: source_paths) {
      std::string key = linker->project->get_unique_path(source_path);
      trace("cache flags for " << key);
      flag_info_cache[key] = info;
    }
  }
  if (can_be_link_command) {
    assert(!have_link_command);
    assert(!link_command.empty());
    linker->set_link_command(link_command);
    link_command.clear();
  }
  // TODO: generate a warning or an error when the link command is never set.
  // Do not run the linker in that case.  Filetree.mk will be invalid, so if it
  // is used, generated an error; otherwise, a warning is enough.
}

std::string sa::FlagExtractor::norm_path(std::string const &path)
{
  auto np = base::os::get_normalized_path(
    base::os::get_absolute_path(
      path.data(),  make_command_info.build_directory().data()
    )
  );
  trace("normalize path " << path << " => " << np);
  return np;
}
  
std::string sa::FlagExtractor::get_build_path(std::string const &abs_path)
{
  assert(base::is_absolute_path(abs_path));
  std::string const &project_directory = linker->project->get_project_path();
  std::string const &build_directory = make_command_info.build_directory();
  if (base::is_nested_in(abs_path, project_directory)) {
    return base::get_relative_path(abs_path, build_directory);
  } else {
    return abs_path;
  }
}

void sa::FlagExtractor::check_target_block_status(Unit *target)
{
  Unit::Lock lock(target);
  _check_target_block_status(target);
}
    
void sa::FlagExtractor::_check_target_block_status(Unit *target)
{
  (void)target;
  check_code(
    bool in_new = find_in_vector(new_targets, target);
    bool in_busy = find_in_vector(busy_targets, target);
    assert_((size_t)(in_new ? 1 : 0) + (size_t)(in_busy ? 1 : 0)
      == target->_get_block_count(),
      in_new << " " << in_busy << " " << target->_get_block_count()
      << " " << target->file->get_name()
    );
  );
}

/*
        // Compiler_regex is the regex used to recognize a compiler command in
        // the output of 'make' using re.search. A valid compiler command is a
        // word that contains a match for this regex anywhere in the word
        // (beginning, middle or end). To match the beginning or end of the word,
        // use an explicit '^' or '$' respectively.  The default recognizes
        // 'gcc', 'g++' and any word that ends in '-gcc' or '-g++'.
        self.compiler_regex = re.compile(
            r'(^|-)(gcc|g\+\+)(\.[eE][xX][eE])?"?$'
        )
        //
        // Ignore_flag_regex is a regex matching compiler flags that should not
        // be passed to Clang, either because they cause undesired side-effects
        // or to avoid the overhead.
        //  -M... flags that tell the compiler to generate dependency files.
        //  -o specifies where the output should go
        //  -m... flags are target specific and control code generation,
        //        with some exceptions
        //  -v and -////// generate verbose output
        //  --save-temps saves temporary files
        //
        // etc. For an overview of options, see for example
        // https://gcc.gnu.org/onlinedocs/gcc/Option-Summary.html
        //
        // Getting this regex right is hard and requires gradual refinement.
        // Here are some options that should be passed to Clang, because they
        // have an effect on errors and warnings:
        //
        //  -fpermissive
        //  -pedantic         strict ISO standard checking
        //  -pedantic-errors
        //  -w                inhibits all warning messages 
        //  -W...             turns warnings into errors or vice versa,
        //                    or suppresses warnings etc
        //  -fmax-errors=n    limits the maximum number of errors emitted
        //  -fsyntax-only     checks for syntax errors only
        //
        // Options controlling the C dialect:
        //
        //  -ansi  -std=standard  -fgnu89-inline 
        //  -fpermitted-flt-eval-methods=standard 
        //  -aux-info filename  -fallow-parameterless-variadic-functions 
        //  -fno-asm  -fno-builtin  -fno-builtin-function  -fgimple
        //  -fhosted  -ffreestanding 
        //  -fopenacc  -fopenacc-dim=geom  -fopenacc-kernels=mode 
        //  -fopenmp  -fopenmp-simd 
        //  -fms-extensions  -fplan9-extensions  -fsso-struct=endianness 
        //  -fallow-single-precision  -fcond-mismatch  -flax-vector-conversions 
        //  -fsigned-bitfields  -fsigned-char 
        //  -funsigned-bitfields  -funsigned-char
        //
        // Options controlling the C++ dialect:
        //
        //  -fabi-version=n  -fno-access-control 
        //  -faligned-new=n  -fargs-in-order=n  -fchar8_t  -fcheck-new 
        //  -fconstexpr-depth=n  -fconstexpr-cache-depth=n 
        //  -fconstexpr-loop-limit=n  -fconstexpr-ops-limit=n 
        //  -fno-elide-constructors 
        //  -fno-enforce-eh-specs 
        //  -fno-gnu-keywords 
        //  -fno-implicit-templates 
        //  -fno-implicit-inline-templates 
        //  -fno-implement-inlines  -fms-extensions 
        //  -fnew-inheriting-ctors 
        //  -fnew-ttp-matching 
        //  -fno-nonansi-builtins  -fnothrow-opt  -fno-operator-names 
        //  -fno-optional-diags  -fpermissive 
        //  -fno-pretty-templates 
        //  -fno-rtti  -fsized-deallocation 
        //  -ftemplate-backtrace-limit=n 
        //  -ftemplate-depth=n 
        //  -fno-threadsafe-statics  -fuse-cxa-atexit 
        //  -fno-weak  -nostdinc++ 
        //  -fvisibility-inlines-hidden 
        //  -fvisibility-ms-compat 
        //  -fext-numeric-literals
        //
        self.ignore_flag_regex = re.compile(
            r'-([Mom].*|[cvES]|###|pipe|wrapper|-version|-target-help|-help.*'
            r'|dumpdir|dumpbase.*|ffile-prefix-map.*|fplugin.*|fdump.*'
            r'|fada-spec-parent'
            r'|fno-fat-lto-objects'
            r')'
        )
        //
        // Some flags match the ignore regex,  but should not be ignored.
        //  -mfloat-abi=... affects the builtin __SOFTFP__ macro
        //                  in clang as well as gcc, in the same way.
        //                  By default, __SOFTFP__ is defined.
        //                  -mfloat-abi=hard undefines __SOFTFP__.
        //
        self.do_not_ignore_flag_regex = re.compile(
            r'-mfloat-abi=.*'
        )
        //
        // Path_flag_regex is a regex that matches all compiler flags that are
        // followed by a path (such as an include path), either directly or in
        // the next argument. Since we cannot actually change directory to the
        // build directory, these paths are made absolute for Clang.
        self.path_flag_regex = re.compile(
            r'-(o|I|isystem|include|imacros|idirafter|iprefix)'
        )
        //
        // Arg_flag_regex is a regex that matches all compiler flags that are
        // followed by an argument (providing extra data for the flag, but not a
        // path) in the next item. The next item should therefore not be
        // interpreted on its own (e.g. as source file) but simply copied to
        // Clang.
        self.arg_flag_regex = re.compile(r'-(x|X.*)')
*/
