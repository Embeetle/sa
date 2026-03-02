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

#include "BinaryAnalyzer.h"
#include "Project.h"
#include "Unit.h"
#include "File.h"
#include "environment.h"
#include "base/os.h"
#include "base/filesystem.h"
#include "base/print.h"
#include "base/debug.h"
#include <string.h>
#include <ctype.h>
#include <regex>
#include <limits>

// What does each column of objdump's Symbol table mean?
// https://stackoverflow.com/questions/6666805

sa::BinaryAnalyzer::BinaryAnalyzer(Unit *unit)
  : Analyzer(unit)
  , source_file(unit->file)
{
  trace("BinaryAnalyzer " << source_file->get_name());
}

sa::BinaryAnalyzer::~BinaryAnalyzer()
{
  trace("~BinaryAnalyzer " << source_file->get_name());
}

bool sa::BinaryAnalyzer::run(const std::string &/*flag_buffer*/)
{
  trace_nest("Run binary code analysis of " << source_file->get_name());
  std::string command_name = project->get_toolchain_prefix() + "nm";
  
  std::vector<const char*> args;
  args.push_back(command_name.data());
  args.push_back("--extern-only"); // -g
  args.push_back("--no-sort");     // -p
  args.push_back("--print-size");  // -S
  args.push_back(source_file->get_path().data());
  std::string build_path;
  {
    Project::Lock lock(project);
    build_path = project->get_build_path();
  }
  trace_atomic_code(
    std::cerr << "\nbinary analysis command: ( cd "
    << base::quote_command_arg(build_path) << " && "
    << base::quote_command_line(args) << " )\n\n"
  );
  args.push_back(0);
  
  int exit_code;
  int error_number = base::os::execute_and_capture(
    args.data(), build_path.data(),
    this, &BinaryAnalyzer::handle_stdout, &BinaryAnalyzer::handle_stderr,
    exit_code, standard_environment
  );
  trace("exit code: " << exit_code);
  if (error_number) {
    report_diagnostic(
      "internal error: binary file analysis failed",
      Severity_error,
      source_file,
      Location(),
      true
    );
  } else if (exit_code) {
    if (!base::is_file(source_file->get_path())) {
      report_diagnostic(
        "file not found: " + source_file->get_path(),
        Severity_error,
        source_file,
        Location(),
        true
      );
    } else if (!base::is_readable(source_file->get_path())) {
      report_diagnostic(
        "file not readable: " + source_file->get_path(),
        Severity_error,
        source_file,
        Location(),
        true
      );
    } else {
      // We may want to do something better here, based on error output from the
      // nm command.
      report_diagnostic(
        "analysis of " + source_file->get_path() + " fails with exit code "
        + base::print(exit_code),
        Severity_error,
        source_file,
        Location(),
        true
      );
    }
  }
  return !error_number;
}

void sa::BinaryAnalyzer::handle_stdout(std::istream &in)
{
  trace_nest("processing nm stdout for " << source_file->get_name());
  std::string value;
  {
    std::stringstream buf;
    buf << in.rdbuf();
    value = buf.str();
  }
  base::os::normalize_line_endings(value);
  set_alternative_content(value);
  
  Section *section = source_file->file_kind == FileKind_object
    ? create_section("") : 0;

  unsigned line_nr = 0;
  size_t end = 0;
  for (size_t offset = 0; offset < value.size(); ++line_nr, offset = end + 1) {
    end = std::min(value.find('\n', offset), value.size());
    std::string line(value.data() + offset, end - offset);

    trace("got line " << line_nr << " @" << offset << ": " << line);
    // Trim trailing white space
    line.erase(line.find_last_not_of(" \n\r\t\v")+1);
    if (line.empty()) {
      trace("line empty");
      continue;
    }
    if (line.back() == ':') {
      std::string member_name = line.substr(0, line.length()-1);
      section = create_section("", member_name);
      trace("enter section " << member_name);
      continue;
    }
    if (!section) {
      trace("no section");
      continue;
    }
    // Find code: single letter surrounded by spaces, and followed by symbol
    // name. Note: code is sometimes preceded by space only, sometimes by
    // address, and sometimes by address and size. Offset is not fixed. For
    // example:
    //
    // 00000000 n wm4.debug.h.24.77018183823075827355cdf15deb618e
    // 00000000 00000032 T Delay_Init
    //          U SystemCoreClock
    //
    size_t code_pos;
    for (code_pos = 1; code_pos < line.size()-2; ++code_pos) {
      if (line[code_pos-1] == ' '
        && isalpha(line[code_pos])
        && line[code_pos+1] == ' '
      ) {
        break;
      }
    }
    if (code_pos >= line.size()-2) {
      trace("no code or no symbol found in line");
      continue;
    }
    char code = line[code_pos];
    size_t begin = code_pos+2;
    std::string name = line.substr(begin);
    Range range(offset + begin, offset + line.length());
    trace("code=" << code << " name=" << name << " " << range);
    //
    // In the output of nm -p -g:
    //   A is a strong symbol with absolute address
    //   B is a strong zero-initialized or uninitialized variable definition
    //   C is a tentative (common) variable definition
    //   D is a strong initialized variable definition
    //   G is a strong initialized variable definition, small object
    //   L is a strong thread-local variable definition
    //   R is a strong initialized variable definition, read-only
    //   S is a strong zero- or uninitialized variable definition, small object
    //   T is a strong function definition
    //   U is a strong symbol reference
    //   u  unique global symbol;  gnu extension
    //   V is a weak variable definition, defaults to zero
    //   v similar to w?
    //   W is a weak function definition
    //   w is a weak function or variable reference
    //
    // The meanings of V, v, W and w have been partially determined
    // experimentally, may be inexact:
    //
    //   V: __attribute__((weak)) int foo;        = weak variable definition
    //   v: never observed                        = weak variable reference?
    //   W: __attribute__((weak)) void foo() {}   = weak function definition
    //   w: __attribute__((weak)) extern int foo; = weak variable reference
    //   w: __attribute__((weak)) void foo();     = weak function reference
    //
    switch (code) {
      case 'A': {
        auto symbol = get_global_symbol(name);
        add_occurrence(
          symbol, OccurrenceKind_definition, OccurrenceStyle_unspecified,
          section, source_file, range, true
        );
        break;
      }
      case 'B':
      case 'D':
      case 'G':
      case 'L':
      case 'R':
      case 'S': {
        auto symbol = get_global_symbol(name);
        add_occurrence(
          symbol, OccurrenceKind_definition, OccurrenceStyle_data,
          section, source_file, range, true
        );
        break;
      }
      case 'C': {
        auto symbol = get_global_symbol(name);
        add_occurrence(
          symbol, OccurrenceKind_tentative_definition,
          OccurrenceStyle_data, section, source_file, range, true
        );
        break;
      }
      case 'T': {
        auto symbol = get_global_symbol(name);
        add_occurrence(
          symbol, OccurrenceKind_definition,  OccurrenceStyle_function,
          section, source_file, range, true
        );
        break;
      }
      case 'U': {
        auto symbol = get_global_symbol(name);
        add_occurrence(
          symbol, OccurrenceKind_use, OccurrenceStyle_unspecified,
          section, source_file, range, true
        );
        break;
      }
      case 'u': {
        // From `man nm`: The symbol is a unique global symbol.  This is a GNU
        // extension to the standard set of ELF symbol bindings.  For such a
        // symbol the dynamic linker will make sure that in the entire process
        // there is just one symbol with this name and type in use.
        //
        // This has been observed with gcc for static data members of a class
        // template. Is this ever used for functions as well?  If not, we can
        // always create a global variable below.
        //
        // We approximate this by a weak definition of a generic (not variable
        // or function) global symbol. Is this correct, or does 'u' cause a link
        // error when there is also a strong definition?
        //
        auto symbol = get_global_symbol(name);
        add_occurrence(
          symbol, OccurrenceKind_weak_definition, OccurrenceStyle_unspecified,
          section, source_file, range, true
        );
        break;
      }
      case 'V': {
        auto symbol = get_global_symbol(name);
        add_occurrence(
          symbol, OccurrenceKind_weak_definition,  OccurrenceStyle_data,
          section, source_file, range, true
        );
        break;
      }
      case 'W': {
        auto symbol = get_global_symbol(name);
        add_occurrence(
          symbol, OccurrenceKind_weak_definition,  OccurrenceStyle_function,
          section, source_file, range, true
        );
        break;
      }
      case 'v':
      case 'w': {
        auto symbol = get_global_symbol(name);
        add_occurrence(
          symbol, OccurrenceKind_weak_use,  OccurrenceStyle_unspecified,
          section, source_file, range, true
        );
        break;
      }
      default: {
      }
    };
  }
}

void sa::BinaryAnalyzer::handle_stderr(std::istream &in)
{
  trace_nest("processing nm stderr for " << source_file->get_name());
  // TODO: extract error message for when nm fails.
  in.ignore(std::numeric_limits<std::streamsize>::max());
}
