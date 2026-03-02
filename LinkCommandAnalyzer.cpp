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

#include "LinkCommandAnalyzer.h"
#include "FileKind.h"
#include "environment.h"
#include "base/filesystem.h"
#include "base/os.h"
#include "base/string_util.h"
#include "base/debug.h"
#include <iostream>
#include <fstream>
#include <string.h>
#include <limits>

sa::LinkCommandAnalyzer::LinkCommandAnalyzer(
  const std::vector<std::string> &command,
  const std::string &build_path,
  const std::string &tmp_path
): build_path(build_path)
{
  trace_nest("LinkCommandAnalyzer");
  assert(!command.empty());
  assert(base::is_absolute_path(tmp_path));
  trace("link command: cd " << base::quote_command_arg(build_path)
    << " && " << base::quote_command_line(command)
  );

  // Buffer for rewritten args
  std::vector<std::string> arg_buffer;
  
  // Create a dry-run command with all relevant flags but only a dummy file.
  std::vector<const char*> dry_run_command;
  dry_run_command.push_back(command[0].data());
  dry_run_command.push_back("-v");
  dry_run_command.push_back("-o");
  dry_run_command.push_back(base::os::get_null_device_path());

  // Examine command line args
  //
  // Note: -l options can only be resolved afterwards, because all -L options
  // need to be taken into account, including those coming after the -l.  Also,
  // order matters for all archives, including those specified using -l and
  // those specified by full path. To handle this, we temporarily insert -l
  // options as such in the file_args list, and later resolve them into their
  // full paths before we return.
  const char *const _L = "-L";
  const char *const _library_path = "--library-path=";
  const char *const _defsym = "--defsym";
  for (size_t i = 1; i < command.size(); ++i) {
    const std::string &arg = command[i];
    trace(arg);
    if (base::begins_with(arg, "-l")) {
      trace("library " << arg);
      if (arg == "-l") {
        if (i+1 < command.size()) {
          ++i;
          trace(command[i]);
          file_args.push_back("-l" + command[i]);
        }
      } else {
        file_args.push_back(arg);
      }
    } else if (base::begins_with(arg, "-")) {
      if (base::begins_with(arg, _L)) {
        trace("library and script search path " << arg);
        dry_run_command.push_back(arg.data());
        if (arg == _L) {
          if (i+1 < command.size()) {
            ++i;
            trace(command[i]);
            dry_run_command.push_back(command[i].data());
            add_lib_search_path(command[i]);
          }
        } else {
          add_lib_search_path(arg.substr(strlen(_L)));
        }
      } else if (base::begins_with(arg, _library_path)) {
        trace("library and script search path " << arg);
        dry_run_command.push_back(arg.data());
        add_lib_search_path(arg.substr(strlen(_library_path)));
      } else if (arg == "-Xlinker") {
        dry_run_command.push_back(arg.data());
        if (i+1 < command.size()) {
          ++i;
          const std::string &arg = command[i];
          trace("linker arg: " << arg);
          //
          // Process --defsym
          // 
          // The gnu linker seems to understand three variants:
          //
          //   --defsym name value
          //   --defsym name=value
          //   --defsym=name=value
          //
          // The llvm linker however only understands the last variant, so the
          // other variants need to be rewritten.
          //
          bool pass_tru = true;
          if (base::begins_with(arg, _defsym)) {
            if (arg == _defsym) {
              if (i+2 < command.size() && command[i+1] == "-Xlinker") {
                const std::string &arg = command[i+2];
                const char *equals = strchr(arg.data(), '=');
                if (equals) {
                  pass_tru = false;
                  arg_buffer.push_back(_defsym + ("=" + arg)); 
                  trace("synthetic arg: " << arg_buffer.back().data());
                  dry_run_command.push_back(arg_buffer.back().data());
                  std::string name(arg, 0, equals-arg.data());
                  trace("defsym: " << name);
                  defsyms.push_back(name);
                  i += 2;
                } else if (i+4 < command.size() && command[i+3] == "-Xlinker") {
                  pass_tru = false;
                  arg_buffer.push_back(
                    _defsym + ("=" + command[i+2] + "=" + command[i+4])
                  );
                  trace("synthetic arg: " << arg_buffer.back().data());
                  dry_run_command.push_back(arg_buffer.back().data());
                  trace("defsym: " << command[i+2]);
                  defsyms.push_back(command[i+2]);
                  i += 4;
                }
              }
            } else if (arg[8] == '=') {
              const char *equals = strchr(arg.data()+9, '=');
              if (equals) {
                std::string name(arg, 9, equals - (arg.data()+9));
                trace("defsym: " << name);
                defsyms.push_back(name);
              }
            }
          }
          if (pass_tru) {
            dry_run_command.push_back(arg.data());
          }
        }
      } else {
        if (base::begins_with(arg, "-Wl,")) {
          if (base::substr_is(arg, 4, "--")) {
            if (base::substr_ends_with(arg, 6, "gc-sections")) {
              trace("set garbage-collect sections");
              garbage_collect_sections = true;
            } else if (base::substr_ends_with(arg, 6, "start-group")) {
              file_args.push_back("(");
            } else if (base::substr_ends_with(arg, 6, "end-group")) {
              file_args.push_back(")");
            }
          }
        }
        // lld does not understand -Wl,-Map=... options.
        // For a dry-run,  we do not want to pass `-o ...`.
        bool pass_tru = !base::begins_with(arg, "-Wl,-Map=") && arg != "-o";
        if (pass_tru) {
          dry_run_command.push_back(arg.data());
        }
        trace("option " << arg << " pass_tru=" << pass_tru);
        static const char *param_args[] = {
          "-o", "-u", "-e", "-Xlinker", "-u", "-z", "-T"
        };
        for (auto param_arg: param_args) {
          if (arg == param_arg) {
            if (i+1 < command.size()) {
              ++i;
              trace(command[i]);
              if (pass_tru) {
                dry_run_command.push_back(command[i].data());
              }
            }
            break;
          }
        }
      }
    } else {
      trace("non option " << arg << " (" << sa::guess_gcc_file_kind(arg)
        << ")"
      );
      if (is_linkable_file(arg)) {
        file_args.push_back(arg);
      } else {
        // This looks like an implicit linker script argument.
        dry_run_command.push_back(arg.data());
      }
    }
  }

  // Execute the linker dry-run command and capture output to find implicit
  // object files, archives and other options added internally by the
  // linker. Implicit archives must be added after explicit archives, to make
  // sure that symbols defined in explicit archives are prioritized over those
  // defined in implicit archives.
  assert(!analyzed);
  int exit_code = 0;
  int error_number = 0;
  {
    // Create empty object file just before executing the dry run command, to
    // decrease the probability that it is deleted in the mean time (race
    // condition). This is important when clearing the SA cache!
    // 
    std::ofstream temp_out;
    empty_o_path = base::open_temp_for_writing(temp_out, "_", ".o");
    if (!temp_out.is_open()) {
      diagnostic_message = "Cannot create " + empty_o_path;
      trace("diagnostic: " << diagnostic_message);
    } else {
      temp_out.close();
      trace("Created empty o path: " << empty_o_path);
      assert(base::path_exists(empty_o_path));
      dry_run_command.push_back(empty_o_path.data());
      dry_run_command.push_back(0);
      trace(
        "\nlinker dry run command: ( cd "
        << base::quote_command_arg(build_path) << " && "
        << "touch " << base::os::quote_command_arg(empty_o_path.data()) << " && "
        << base::os::quote_command_line(dry_run_command.data()) << " )\n"
      );
      error_number = base::os::execute_and_capture(
        dry_run_command.data(), build_path.data(), this,
        &LinkCommandAnalyzer::handle_stdout,
        &LinkCommandAnalyzer::handle_stderr,
        exit_code, standard_environment
      );
      if (!analyzed) {
        trace("Could not analyze link command");
        if (error_number) {
          if (error_number == ENOENT
            && command[0].find('/') == std::string::npos
          ) {
            diagnostic_message = "Link command "
              + base::quote_command_arg(command[0])
              + " not found on PATH";
          } else {
            diagnostic_message = "Link command "
              + base::quote_command_arg(command[0]) + ": "
              + base::os::strerror(error_number);
          }
        } else {
          diagnostic_message = "Link command "
            + base::quote_command_line(command)
            + " fails with exit code "
            + std::to_string(exit_code);
        }
        trace("diagnostic: " << diagnostic_message);
      }
    }
  }
  trace("error_number: " << error_number);
  trace("exit code: " << exit_code);
  trace("analyzed: " << analyzed);
  remove(empty_o_path.data());
  // Note: when the linker command cannot be analyzed, -l options in file args
  // must still be resolved, or else the file args list must be cleared. Calling
  // code does not expect -l options in the file args list.
  std::vector<std::string> resolved_file_args;
  for (auto const &arg: file_args) {
    if (base::begins_with(arg, "-l")) {
      trace_nest("resolve library " << arg);
      bool found = false;
      for (auto dir: lib_search_path) {
        std::string path = dir + "/lib" + (arg.data()+2) + ".a";
        if (base::is_file(path)) {
          trace("found " << path);
          found = true;
          resolved_file_args.emplace_back(path);
          break;
        }
      }
      if (!found) {
        trace("not found: " << arg);
        unresolved_libs.emplace_back(arg);
      }
    } else {
      trace("copy " << arg);
      resolved_file_args.emplace_back(arg);
    }
  }
  file_args.swap(resolved_file_args);
  {
    trace_nest("file args:");
    for (auto arg: file_args) {
      trace(arg);
    }
  }
  {
    trace_nest("unresolved libs:");
    for (auto arg: unresolved_libs) {
      trace(arg);
    }
  }
  {
    trace_nest("search path:");
    for (auto dir: lib_search_path) {
      trace(dir);
    }
  }
  {
    trace_nest("script analysis args:");
    for (auto arg: script_analysis_args) {
      trace(arg);
    }
  }
}

void sa::LinkCommandAnalyzer::add_lib_search_path(std::string const &path)
{
  lib_search_path.push_back( base::get_absolute_path(path, build_path) );
}

namespace {
  // Linker command line parser. This parses the "collect2" command line as
  // outputted by the -v option. Special care is taken to handle the case
  // where the toolchain is located on a path containing a space.
  class Parser {
    const char *pos;

    // A prefix of the toolchain path containing all spaces in that path.
    const char *base;
    size_t base_size;

  public:
    std::vector<std::string> &file_args;
    std::vector<std::string> &lib_search_path;
    std::vector<std::string> &script_analysis_args;
    const std::string &empty_o_path;
        
    Parser(
      const char *line,
      std::vector<std::string> &file_args,
      std::vector<std::string> &lib_search_path,
      std::vector<std::string> &script_analysis_args,
      const std::string &empty_o_path
    )
      : pos(line), base(pos), base_size(0)
      , file_args(file_args)
      , lib_search_path(lib_search_path)
      , script_analysis_args(script_analysis_args)
      , empty_o_path(empty_o_path)
    {
    }

    bool parse()
    {
      trace("initial lib search path: " << lib_search_path);
      trace("empty o path: " << empty_o_path);
      trace_nest("parse collect2: " << pos);
      static const std::string key = std::string("/collect2")
        + ::base::os::get_exe_extension();
      trace("key: " << key);
      static const unsigned key_size = key.length();
      pos += key_size;
      for (;;) {
        pos = strchr(pos, ' ');
        if (!pos) {
          trace("not a linker command (no space after " << key);
          return false;
        }
        trace("try " << std::string(pos).substr(0,60));
        if (!strncmp(pos-key_size, key.data(), key_size)) {
          trace("found key " << key);
          break;
        }
        ++pos;
        // The base must stop at the last space, so that other folders in the
        // toolchain directory are handled correctly.
        base_size = pos - base;
      }
      trace("base: '" << std::string(base, base_size) << "'");
      while (*pos) {
        skip_space();
        trace_nest("handle " << std::string(pos).substr(0,120));
        const char *begin = pos;
        if (base::os::is_absolute_path(pos)) {
          // This could be the empty o path.  Treat that separately, because it
          // might contain spaces, and it is not in the toolchain folder.
          if (!strncmp(pos, empty_o_path.data(), empty_o_path.size())
            && ( !pos[empty_o_path.size()] || pos[empty_o_path.size()] == ' ')
          ) {
            pos += empty_o_path.size();
            trace("skip empty o path " << empty_o_path);
          } else {
            skip_path_arg();
            std::string path(begin, pos-begin);
            trace("path: " << path);
            if (pos[-2] == '.' && (pos[-1] == 'o' || pos[-1] == 'a')) {
              if (path != empty_o_path) {
                file_args.push_back(path);
                // TODO: don't add file args as script analysis arg.  Instead,
                // generate and add a single dummy object file.
                add_script_analysis_arg(begin, pos);
              }
            }
          }
        } else if (skip_prefix("-L")) {
          const char *begin_path = pos;
          skip_path_arg();
          trace("lib search dir: " << std::string(begin_path, pos-begin_path));
          lib_search_path.push_back(std::string(begin_path, pos-begin_path));
          add_script_analysis_arg(begin, pos);
        } else if (skip_prefix("-l")) {
          skip_arg();
          trace("lib name: " << std::string(begin, pos-begin));
          file_args.emplace_back(begin, pos-begin);
        } else if (skip_key("-plugin") || skip_key("-o")) {
          trace("after skip -plugin or -o: " << std::string(pos).substr(0,60));
          skip_space();
          skip_path_arg();
        } else if (skip_key("--start-group")) {
          trace("after skip --start-group");
          file_args.push_back("(");
        } else if (skip_key("--end-group")) {
          trace("after skip --end-group");
          file_args.push_back(")");
        } else if (skip_prefix("-plugin-opt=")) {
          trace("after skip -plugin-opt: " << std::string(pos).substr(0,60));
          skip_assign();
          skip_path_arg();
        } else if (skip_prefix("--sysroot=")) {
          trace("after skip --sysroot: " << std::string(pos).substr(0,60));
          skip_path_arg();
          add_script_analysis_arg(begin, pos);
        } else if (skip_key(empty_o_path.data())) {
          // Empty o path is used for dry run command but not for script
          // analysis.
          trace("after skip empty path: " << std::string(pos).substr(0,60));
        } else if (skip_key("-flto")
          || ( skip_prefix("-mavr") ? (skip_arg(), true) : false )
        ) {
          trace("skip " << std::string(begin, pos));
        } else {
          skip_path_arg();
          trace("after skip arg: " << std::string(pos).substr(0,60));
          add_script_analysis_arg(begin, pos);
        }
      }
      trace("final lib search path: " << lib_search_path);
      trace("final script analysis args: " << script_analysis_args);
      return true;
    }

    void add_script_analysis_arg(const char *begin, const char *end)
    {
      script_analysis_args.emplace_back(begin, end-begin);
    }

    bool skip_base()
    {
      if (strncmp(pos, base, base_size)) return false;
      pos += base_size;
      return true;
    }

    bool skip_prefix(const char *prefix)
    {
      size_t len = strlen(prefix);
      if (strncmp(pos, prefix, len)) return false;
      pos += len;
      return true;
    }

    bool skip_key(const char *key)
    {
      size_t len = strlen(key);
      if (strncmp(pos, key, len)) return false;
      if (pos[len] && pos[len] != ' ') return false;
      pos += len;
      return true;
    }

    // Skip the argument at the current position.  The argument ends at the
    // next space.
    void skip_arg()
    {
      pos = get_end();
    }

    // Skip any non-space characters followed by '='. This is used to handle
    //   -plugin-opt=-fresolution=C:\Users\Bindiya Madhu\...
    // and similar cases where the '=' is followed by a path possibly containing
    // spaces that needs to be skipped.
    void skip_assign()
    {
      for (const char *p = pos; *p && *p != ' '; ++p) {
        if (*p == '=') {
          pos = p+1;
          break;
        }
      }
    }

    // Skip the argument at the current position.  The argument ends at the next
    // space, except if it starts with a special path: then any spaces in the
    // special path will be skipped too. On Windows, the argument can contain a
    // backslash where the special path has a /.
    //
    // Special paths are the base path and the temp directory path.
    void skip_path_arg()
    {
      static const char *tmpdir = base::os::get_temp_directory_path();
      static size_t tmpdir_size = strlen(tmpdir);
      trace("tmpdir: " << tmpdir);
      if (base::os::starts_with_path(pos, base, base_size)) {
        trace("skipped base path");
        pos += base_size;
      } else if (base::os::starts_with_path(pos, tmpdir, tmpdir_size)) {
        trace("skipped tmpdir path");
        pos += tmpdir_size;
      } else {
        trace("no special path: " << std::string(pos).substr(0,60));
      }
      skip_arg();
    }

    // Find the position of the next space or null character.
    const char *get_end()
    {
      const char *end = strchr(pos, ' ');
      return end ? end : pos + strlen(pos);
    }

    void skip_space()
    {
      while (*pos && *pos == ' ') ++pos;
    }
  };
}

#ifdef SELFTEST
int main()
{
  debug_writeln("temp dir: " << base::os::get_temp_directory_path());
  base::os::set_temp_directory_path("C:/Users/Bindiya Madhu/AppData/Local/Temp");
  debug_writeln("temp dir: " << base::os::get_temp_directory_path());

  static const char *tmpdir = base::os::get_temp_directory_path();
  static size_t tmpdir_size = strlen(tmpdir);
  assert(base::os::starts_with_path(
      "C:/Users/Bindiya Madhu/AppData/Local/Temp/ccgrYonM.res",
      tmpdir, tmpdir_size
    )
  );
  std::vector<std::string> file_args;
  std::vector<std::string> raw_lib_search_path;
  std::vector<std::string> script_analysis_args;

  std::string line = std::string() + "c:/embeetle/src/embeetle"
    " ide/embeetle/beetle_tools/windows/gnu_riscv_xpack_toolchain_12.2.0_3_32b"
    "/bin/../libexec/gcc/riscv-none-elf/12.2.0/collect2"
    + base::os::get_exe_extension() +
    " -plugin"
    " c:/embeetle/src/embeetle ide/embeetle/beetle_tools/windows"
    "/gnu_riscv_xpack_toolchain_12.2.0_3_32b/bin/../libexec/gcc/riscv-none-elf"
    "/12.2.0/liblto_plugin.dll -plugin-opt=c:/embeetle/src/embeetle"
    " ide/embeetle/beetle_tools/windows/gnu_riscv_xpack_toolchain_12.2.0_3_32b"
    "/bin/../libexec/gcc/riscv-none-elf/12.2.0/lto-wrapper.exe"
    " -plugin-opt=-fresolution=C:/Users/Bindiya Madhu/AppData/Local/Temp"
    "/ccgrYonM.res -plugin-opt=-pass-through=-lgcc"
    " -plugin-opt=-pass-through=-lg_nano -plugin-opt=-pass-through=-lc_nano"
    " -plugin-opt=-pass-through=-lgcc -plugin-opt=-pass-through=-lgcc"
    " -plugin-opt=-pass-through=-lc_nano -plugin-opt=-pass-through=-lnosys"
    " -plugin-opt=-pass-through=-lgcc -plugin-opt=-pass-through=-lc_nano"
    " -plugin-opt=-pass-through=-lnosys"
    " --sysroot=c:/embeetle/src/embeetle ide/embeetle/beetle_tools/windows"
    "/gnu_riscv_xpack_toolchain_12.2.0_3_32b/bin/../riscv-none-elf"
    " -melf32lriscv -o NUL -L../config/"
    " -Lc:/embeetle/src/embeetle ide/embeetle/beetle_tools/windows"
    "/gnu_riscv_xpack_toolchain_12.2.0_3_32b/bin/../lib/gcc/riscv-none-elf"
    "/12.2.0/rv32ec/ilp32e -Lc:/embeetle/src/embeetle ide/embeetle/beetle_tools"
    "/windows/gnu_riscv_xpack_toolchain_12.2.0_3_32b/bin/../lib/gcc"
    "/riscv-none-elf/12.2.0/../../../../riscv-none-elf/lib/rv32ec/ilp32e"
    " -Lc:/embeetle/src/embeetle ide/embeetle/beetle_tools/windows"
    "/gnu_riscv_xpack_toolchain_12.2.0_3_32b/bin/../riscv-none-elf/lib/rv32ec"
    "/ilp32e -Lc:/embeetle/src/embeetle ide/embeetle/beetle_tools/windows"
    "/gnu_riscv_xpack_toolchain_12.2.0_3_32b/bin/../lib/gcc/riscv-none-elf"
    "/12.2.0 -Lc:/embeetle/src/embeetle ide/embeetle/beetle_tools/windows"
    "/gnu_riscv_xpack_toolchain_12.2.0_3_32b/bin/../lib/gcc"
    " -Lc:/embeetle/src/embeetle ide/embeetle/beetle_tools/windows"
    "/gnu_riscv_xpack_toolchain_12.2.0_3_32b/bin/../lib/gcc/riscv-none-elf"
    "/12.2.0/../../../../riscv-none-elf/lib -Lc:/embeetle/src/embeetle"
    " ide/embeetle/beetle_tools/windows/gnu_riscv_xpack_toolchain_12.2.0_3_32b"
    "/bin/../riscv-none-elf/lib --gc-sections C:/Users/Bindiya Madhu/AppData"
    "/Local/Temp/_41.o -lgcc -lg_nano -lc_nano -lgcc --start-group -lgcc"
    " -lc_nano -lnosys --end-group --start-group -lgcc -lc_nano -lnosys"
    " --end-group -T ../config/linkerscript.ld";
  std::string empty_o_path = "C:/Users/Bindiya Madhu/AppData/Local/Temp/_41.o";
  Parser parser(line.data(), file_args, raw_lib_search_path,
    script_analysis_args, empty_o_path
    
  );
  parser.parse();
  assert_(script_analysis_args[0] ==
    "--sysroot=c:/embeetle/src/embeetle ide/embeetle/beetle_tools/windows"
    "/gnu_riscv_xpack_toolchain_12.2.0_3_32b/bin/../riscv-none-elf",
    script_analysis_args[0]
  );
  return 0;
}

#endif

void sa::LinkCommandAnalyzer::handle_stdout(std::istream &in)
{
#if 1
  in.ignore(std::numeric_limits<std::streamsize>::max());
#else
  // Expect no standard output; everything is written to standard error.
#if 1
  // Do not attempt to read standard output; it causes a deadlock on Windows as
  // described in handle_stderr. Still not clear what happens precisely.
  (void)in;
#else
  // To avoid deadlock in case output is produced, read it.
  std::string line;
  while (std::getline(in, line)) {
    trace("got out: " << line);
  }
#endif
#endif
}

void sa::LinkCommandAnalyzer::handle_stderr(std::istream &in)
{
  trace_nest("Linker handle stderr");
  // The current line of command output being parsed.
  std::string line;
  //
  // On Windows, the getline call below will sometimes "hang" at the end of the
  // stream: instead of returning false (end-of-file), it waits forever. This
  // seems to happen only when the compilation commands in the makefile do not
  // contain '-c', so that the SA recognizes multiple link commands. Not sure if
  // this is relevant, but the hang does not occur on the first link command,
  // only on later ones, which happen to run in parallel with binary analysis
  // commands.
  //
  // Test using symkinds.test (or some other tests).
  //
  // Just before the hanging getline call, in.eof()==0 in.good()==1 in.fail()==0
  // in.bad()==0, so everything seems fine (but isn't).
  //
  // Setting EMBEETLE_MAX_THREADS to 1 avoids the error (but slows down
  // analysis).  
  //
  // As a work-around, we stop processing as soon as the necessary information
  // has been found; this is more efficient anyway.
  //
  // An alternative work-around is to not lock the project while executing the
  // linker command. The fact that this works suggests that this is a deadlock
  // problem. Possibly, reading the eof of a pipe without producer locks some OS
  // mutex. We then have this thread locking first the project mutex and then
  // the OS mutex, while other threads (for binary analysis) lock first the OS
  // mutex and then the project mutex.
  //
  // Each of these two work-arounds seems to fix the problem.  Both have been
  // applied below.
  //
  std::vector<std::string> raw_lib_search_path;
  while (std::getline(in, line)) {
    // Input stream is in binary mode, not text mode, so \r can occur at the
    // end of the line. This usually doesn't hurt because it is whitespace, but
    // causes terribly confusing trace output when the line is printed followed
    // by more data. Specifically, the output before the \r is erased.
    if (!line.empty() && line.back() == '\r') line.pop_back();
    trace("got err: " << line);
    if (*line.data() == ' ') {
      trace("process line: " << line);
      Parser parser(line.data()+1, file_args, raw_lib_search_path,
        script_analysis_args, empty_o_path
      );
      if (parser.parse()) {
        analyzed = true;
        // Break here required as workaround for getline bug (described at head
        // of loop).
        break;
      }
    }
  }
  for (auto path: raw_lib_search_path) {
    add_lib_search_path(path);
  }
  in.ignore(std::numeric_limits<std::streamsize>::max());
}
