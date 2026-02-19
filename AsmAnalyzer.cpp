// Copyright 2018-2024 Johan Cockx
#include "AsmAnalyzer.h"
#include "Project.h"
#include "Unit.h"
#include "LineOffsetTable.h"
#include "base/os.h"
#include "base/filesystem.h"
#include "base/print.h"
#include "base/debug.h"
#include <string.h>
#include <ctype.h>
#include <regex>

#include <stdio.h>
#include <string.h>

sa::AsmAnalyzer::AsmAnalyzer(Unit *unit)
  : ExternalAnalyzer(unit)
{
}

sa::AsmAnalyzer::~AsmAnalyzer()
{
  trace_nest("~AsmAnalyzer " << (void*)this);
}

bool sa::AsmAnalyzer::run(const std::string &flag_buffer)
{
  trace_nest("Run ASM analysis of " << initial_source_file->get_name());
  trace("Flag buffer size: " << flag_buffer.size());
  {
    std::ifstream source(initial_source_file->get_path().data());
    if (!source) {
      Analyzer::report_diagnostic(
        "assembly source file " + initial_source_file->get_path() + " not found",
        Severity_error,
        initial_source_file,
        Location(),
        true
      );
      return false;
    }
  }
  //
  // We will run an external analyzer for assembly code and read its output from
  // a pipe. The list of arguments can be very long for projects with many
  // subdirectories, so we will put them in a file to be loaded from the command
  // line with an '@' arg.
  std::string flags_file_path;
  {
    std::ofstream out;
    flags_file_path = base::open_temp_for_writing(out, "", ".flags");
    trace("ffpath: " << flags_file_path);
    assert_(out.is_open(), "cannot write " << flags_file_path);

    // Add flags from flag buffer to flags file.  Flag buffer contains
    // null-separated flag values.
    const char *end = flag_buffer.data() + flag_buffer.size();
    for (const char *flag = flag_buffer.data(); flag < end;
         flag += strlen(flag) + 1
    ) {
      out << base::quote_command_arg(flag) << "\n";
    }
  }
  //
  // Build the command line for the external analyzer.
  std::string const analyzer_path = project->get_asm_analyzer_path();
  std::vector<const char*> args;
  args.push_back(analyzer_path.data());

  // If the gcc assembler gets command line arguments that are irrelevant for
  // the assembler, it just ignores them. Clang, however, emits a warning by
  // default, which may be turned into an error when -Werror is used.  So, to be
  // consistent with gcc, we pass -Wno-unused-command-line-argument.
  //
  // Examples of unused command line arguments are:
  //   -fmessage-length=0
  //   -ffunction-sections
  //   -fdata-sections
  //   -fomit-frame-pointer
  //
  // as well as preprocessor (un)defines for plain - non-preprocessed -
  // assembly:
  //   -U __has_c_attribute
  //   -U __i386__
  //   -D ARM_MATH_CM7
  //
  args.push_back("-Wno-unused-command-line-argument");

  // With -canonical-prefixes (the default), clang will fail with the following
  // error on Windows:
  //
  //    clang: error: unable to execute command: program not executable
  //
  // The relevant Clang source code is in llvm/clang/tools/driver/driver.cpp in
  // the function GetExecutablePath, which returns "clang" instead of the full
  // path of the Clang executable. While this might work when the correct
  // version of Clang is on your PATH, it certainly doesn't work for us.
  //
  // The relevant online documentation does not really clarify the situation:
  //
  //     By default, clang will internally use absolute paths to refer to
  //     compiler-internal headers. Pass -no-canonical-prefixes to make clang
  //     use relative paths for these internal files.
  // [https://blog.llvm.org/2019/11/deterministic-builds-with-clang-and-lld.html]
  //
  // Anyway, passing -no-canonical-prefixes fixes this issue.
  //
  args.push_back("-no-canonical-prefixes");

  // Add `-U __has_c_attribute` to handle a difference between gcc and clang.
  //
  // __has_cpp_attribute is a predefined function-like macro in at least gcc
  // 9, possibly earlier. It can be used to test for the presence of certain
  // C++ features. From the gcc documentation:
  //
  // The special operator __has_c_attribute (operand) may be used in ‘#if’ and
  // ‘#elif’ expressions in C code to test whether the attribute referenced by
  // its operand is recognized by GCC in attributes using the ‘[[]]’
  // syntax. GNU attributes must be specified with the scope ‘gnu’ or
  // ‘__gnu__’ with __has_c_attribute. When operand designates a supported
  // standard attribute it evaluates to an integer constant of the form YYYYMM
  // indicating the year and month when the attribute was first introduced
  // into the C standard, or when the syntax of operands to the attribute was
  // extended in the C standard.
  // https://gcc.gnu.org/onlinedocs/cpp/_005f_005fhas_005fc_005fattribute.html
  //
  // __has_c_attribute is a similar macro for C code. It did not exist yet in
  // gcc 9, but certainly exists in gcc 11.
  //
  // Both __has_cpp_attribute and __has_c_attribute are undefined in gcc when
  // preprocessing assembly code (with .S extension or -x
  // assembler-with-cpp). This fact is used in our CIP project
  // cip-united-pic32mz1024efg144, where a C header file is included from
  // assembly code. The header file contains the following code:
  //
  //    #if defined (__has_c_attribute) || defined (__has_cpp_attribute)
  //    extern void *_ebase_address;
  //    #define EBASE_ADDR ((int)&_ebase_address)
  //    #else
  //    #define EBASE_ADDR _ebase_address
  //    #endif
  //
  // Since `extern void *_ebase_address;` is not a valid assembly statement,
  // this only compiles when __has_cpp_attribute and __has_c_attribute are
  // undefined when preprocessing assembly code.
  //
  // Unfortunately, clang has a different strategy. It defines
  // __has_c_attribute for C and assembly, and defines __has_cpp_attribute for
  // C++. This was tested in version 17.0.0.
  //
  //                                gcc 9     gcc 11     clang 17
  // __has_c_attribute in C++          no        yes           no
  // __has_cpp_attribute in C++       yes        yes          yes
  // __has_c_attribute in C            no        yes          yes
  // __has_cpp_attribute in C         yes        yes           no
  // __has_c_attribute in ASM          no         no          yes
  // __has_cpp_attribute in ASM        no         no           no
  //
  args.push_back("-U");
  args.push_back("__has_c_attribute");
  //
  // We currently do not know the position of the source file with respect to
  // the other arguments. We do know, however, that an argument like `-x
  // assembler-with-cpp` must come before the source file to have an
  // effect. Therefore we add the flags file before the source file on the
  // command line. If this heuristic causes problems, we will have to keep track
  // of the position of the source file in the original command.
  std::string include_flags_file_arg = "@" + flags_file_path;
  args.push_back(include_flags_file_arg.data());
  args.push_back("-c");
  args.push_back(initial_source_file->get_path().data());
  debug_atomic_writeln("asm command: " << base::quote_command_line(args));
  args.push_back(0);

  // TODO: use project path below?
  int exit_code = ExternalAnalyzer::run_external_command(args.data(), ".");
  bool success = exit_code == 0 || exit_code == 1;
  if (!success) {
    report_diagnostic(
      "internal error: exit code " + base::print(exit_code) + " from "
      + base::quote_command_line(args),
      Severity_fatal, 0, Location(), true
    );
  }
  return success;
}

void sa::AsmAnalyzer::handle_stderr(std::istream &in)
{
  trace_nest("processing asm stderr");
  std::string line;
  while (std::getline(in, line)) {
    handle_asm_error_line(line);
  }
}

// Determine if this is a linker relevant diagnostic, i.e. a diagnostic that
// affects the list of global symbols defined in this translation unit.
// Determine if this is a linker relevant diagnostic, i.e. a diagnostic that
// affects the list of global symbols defined in this translation unit.
bool sa::AsmAnalyzer::is_linker_relevant_diagnostic(
  sa::Severity severity,
  const std::string &message
)
{
  // Currently, we assume that any error is linker relevant unless it matches a
  // regular expression for known non-linker-relevant errors. The regular
  // expression may need to be extended in the future.
  static std::regex safe_errors_regex(
    "invalid instruction.*"
    "|too few operands for instruction.*"
    "|unexpected token in operand.*"
    "|unexpected token in argument list.*"
    , std::regex::optimize
  );
  return severity >= sa::Severity_error
    && !regex_match(message, safe_errors_regex);
}

static bool is_bogus_diagnostic(
  sa::Severity severity,
  const std::string &message
)
{
  // Regex must match the whole diagnostic message.
  static std::regex bogus_warnings_regex(
    // ..: warning: new target does not support arm mode, switching to thumb mode
    //     .arch armv7e-m
    //     ^
    // These warnings are caused by an incompatibility between gas and llvm: see
    // https://reviews.llvm.org/D18955
    "new target does not support arm mode, switching to thumb mode"
    "|"
    // Warning generated because we force all gcc internal macro definitions
    // onto llvm.
    "undefining builtin macro \\[-Wbuiltin-macro-redefined\\]"
    //
    , std::regex::optimize
  );
  if (severity == sa::Severity_warning) {
    if (regex_match(message, bogus_warnings_regex)) {
      return true;
    }
  }
  return false;
}

static unsigned long safe_dec(unsigned long value)
{
  return value ? value-1 : value;
}

void sa::AsmAnalyzer::handle_asm_error_line(const std::string &text)
{
  trace("Handle stderr line: " << text);

  static std::regex diagnostic_regex(
    "(.+):([1-9][0-9]*):([1-9][0-9]*): (warning|error): (.*)\r?"
    , std::regex::optimize
  );
  std::smatch match_results;
  if (!regex_match(text, match_results, diagnostic_regex)) {
    trace("Not a diagnostic");
    return;
  }
  Severity severity = severity_by_name(match_results[4]);
  std::string message = match_results[5];
  trace("Diagnostic: " << message);

  if (is_bogus_diagnostic(severity, message)) {
    trace("Bogus diagnostic");
    return;
  }

  base::ptr<File> file = get_file(match_results[1]);
  unsigned line = safe_dec(std::stoul(match_results[2]));
  unsigned column = safe_dec(std::stoul(match_results[3]));
  unsigned offset = get_offset(file, line, column);
  Analyzer::report_diagnostic(message, severity, file, Location(offset),
    is_linker_relevant_diagnostic(severity, message)
  );
}
