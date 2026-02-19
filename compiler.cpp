#include "compiler.h"
#include "Lockable.h"
#include "environment.h"
#include "base/string_util.h"
#include "base/filesystem.h"
#include "base/os.h"
#include <charconv>
#include <map>
#include "base/debug.h"

// Check if this flag potentially affects built-in compiler macros. For
// example, -mfloat-abi=hard affects whether __SOFTFP__ is defined as a
// built-in macro.
//
// -m* are target specific options
// -f* and -std=* options control the C++ dialect
//
// Probably, not all of these affect builtin defines. On the other hand,
// these options typically do not change from file to file, and since we
// are using a map to cache builtin defines, using too many to extract
// builtin flags does not hurt much.
//
bool sa::is_compiler_builtin_relevant(std::string const &flag)
{
  return flag[0] == '-' && (
    flag[1] == 'm' || flag[1] == 'f'
    || ((std::string_view)flag).substr(1,3) == "std"
  );
}

namespace {
  static inline std::string_view substr(
    std::string const &value, size_t pos=0, size_t len=std::string::npos
  ) {
    return static_cast<std::string_view>(value).substr(pos, len);
  }

  struct CompilerKey {
    std::string compiler;
    sa::FileKind source_kind;
    std::vector<std::string> relevant_flags;

    CompilerKey(
      std::string_view compiler,
      sa::FileKind source_kind,
      std::vector<std::string> const &relevant_flags
    )
      : compiler(compiler)
      , source_kind(source_kind)
      , relevant_flags(relevant_flags)
    {
      trace("compiler key for " << compiler << " created");
      trace("relevant flags: " << relevant_flags);
    }

    ~CompilerKey()
    {
      trace("compiler key for " << compiler << " destroyed");
    }

    bool operator<(const CompilerKey &other) const
    {
      if (compiler < other.compiler) return true;
      if (compiler > other.compiler) return false;
      if (source_kind < other.source_kind) return true;
      if (source_kind > other.source_kind) return false;
      return relevant_flags < other.relevant_flags;
    }
  };
  
  struct CompilerRunner
  {
    CompilerKey const &key;
    sa::CompilerResults &results;

    CompilerRunner(CompilerKey const &key, sa::CompilerResults &results)
      : key(key), results(results)
    {
      trace_nest("Create compiler runner for " << key.compiler << " for "
        << key.source_kind
      );
      for (auto const &flag: key.relevant_flags) {
        trace("relevant flag: " << flag);
      }
    }
    
    void run()
    {
      // To extract built-in compiler flags, we run the compiler with -v
      // (verbose), -dM (show all macro definitions, including built-in ones),
      // -E (preprocessor, required in combination with -dM), -x <language> and
      // relevant user flags on an existing empty file, like /dev/null on Linux
      // or NUL on Windows.
      //
      // The value of <language> depends on the source file extension.
      //
      // With any gcc-like compiler, this will produce output that includes the
      // target name and a list of system include directories on stderr, and a
      // list of predefined macros on stdout.
      //
      std::vector<const char*> command;
      command.push_back(key.compiler.data());
      command.push_back("-v");
      command.push_back("-dM");
      command.push_back("-E");
      command.push_back("-x");
      switch (key.source_kind) {
        case sa::FileKind_assembler:
          command.push_back("assembler");
          break;
        case sa::FileKind_assembler_with_cpp:
          command.push_back("assembler-with-cpp");
          break;
        case sa::FileKind_c:
          command.push_back("c");
          break;
        case sa::FileKind_cplusplus:
          command.push_back("c++");
          break;
        default:
          std::ostringstream message;
          message << "unsupported: analysis of " << key.source_kind
                  << " source file";
          add_error(message.str(), sa::Category_toolchain);
          return;
      }
      command.push_back(base::os::get_null_device_path());
      for (auto const &flag: key.relevant_flags) {
        trace("push relevant flag: " << flag);
        command.push_back(flag.data());
      }
      command.push_back(0);
      trace("command to extract compiler builtin flags: "
        << base::os::quote_command_line(command.data())
      );
      results.valid = true;
      int exit_code = 0;
      int error_number = base::os::execute_and_capture(
        command.data(), ".", this, &CompilerRunner::handle_stdout,
        &CompilerRunner::handle_stderr, exit_code, sa::standard_environment
      );
      trace(error_number << "/" << exit_code << "/" << results.valid);
      if (error_number) {
        std::ostringstream message;
        message << "cannot execute " << command[0] << ": "
                << base::os::strerror(error_number);
        add_error(message.str(), sa::Category_toolchain);
      } else if (exit_code && results.valid) {
        // Add last resort diagnostic if no errors were detected in the
        // compiler's error output.
        std::ostringstream message;
        message << "exit code " << exit_code << " from " << command[0];
        add_error(message.str(), sa::Category_makefile);
      }
      if (results.valid) {
        // Builtin flags consist of target flags, builtin defines and builtin
        // includes.

        // Target flag needs to be patched for Clang.
        fix_target(target);

        // Even if no target is set, Clang sometimes (only on Windows?) defines
        // __i386__, which confuses the header files for the AVR toolchain
        // (target: 'avr') and possibly also the header files for other
        // toolchains.  Undefine it explicitly.  This is for now the only such
        // preprocessor define that we discovered, but there may be more.
        //
        // TODO: is this still an issue?        
        add_flag("-U__i386__");

        add_pointer_size_flag();

        // Even if Clang accepts a target, it does not define all the
        // preprocessor definitions that gcc defines for this target. One
        // example is __VFP_FP__, which is defined to 1 in gcc for the
        // arm-none-eabi target and undefined in Clang. For this reason, builtin
        // defines are added even for a target accepted by Clang.
        for (auto const &[header, value]: macros) {
          add_macro_definition(header, value);
        }

        // Add system includes, from the toolchain directory; Clang does not
        // know the toolchain directory so does not add these.
        for (auto const &system_include: system_includes) {
          add_flag("-isystem");
          add_flag(base::get_normalized_path(system_include));
        }
      }
    }
    
  protected:
    std::string target = "";
    std::vector<std::pair<std::string,std::string>> macros;
    std::vector<std::string> system_includes;

    // Parse compiler output
    void handle_stdout(std::istream &in)
    {
      trace_nest("handle stdout for compiler built-in flags");

      std::string line;
      for (;;) {
        std::getline(in, line);
        if (!in) break;
        base::os::normalize_line_endings(line);
        trace("output line: " << line);
        static const char define_marker[] = "#define ";
        if (base::begins_with(line, define_marker)) {
          size_t begin = sizeof(define_marker)-1;
          size_t pos = line.find(' ', begin);
          if (pos == line.npos) {
            macros.emplace_back(substr(line, begin), "");
          } else {
            macros.emplace_back(substr(line, begin, pos-begin),
              substr(line, pos+1)
            );
          }
        }
      }
    }

    void handle_stderr(std::istream &in)
    {
      trace_nest("handle stderr for compiler built-in flags");
      std::string line;
      bool in_system_includes = false;
      for (;;) {
        std::getline(in, line);
        if (!in) break;
        base::os::normalize_line_endings(line);
        if (in_system_includes) {
          if (line[0] != ' ') {
            in_system_includes = false;
            continue;
          }
          system_includes.emplace_back(line.substr(1));
          continue;
        }
        if (line == "#include <...> search starts here:") {
          in_system_includes = true;
          continue;
        }
        static const char target_marker[] = "Target: ";
        if (base::begins_with(line, target_marker)) {
          target = line.substr(sizeof(target_marker)-1);
          trace("found target: " << target);
          continue;
        }
        static const char error_marker[] = " error: ";
        size_t colon_pos = line.find(':');
        trace("error line: " << line << " " << (colon_pos != line.npos));
        if (colon_pos != line.npos) {
          if (substr(line, colon_pos+1, sizeof(error_marker)-1)
          == error_marker
          ) {
            trace("got error");
            std::ostringstream message;
            message << substr(line, colon_pos+sizeof(error_marker))
                    << " (" << substr(line, 0, colon_pos) << ")";
            add_error(message.str(), sa::Category_makefile);
          }
        }
      }
    }

    void add_error(std::string_view message, sa::Category category)
    {
      results.diagnostics.emplace_back(message, sa::Severity_fatal, category);
      results.valid = false;
    }

    // Add target flag to results. Target flag needs to be patched for Clang.
    void fix_target(std::string target) {
      if (target.empty()) return;
      if (target == "riscv-none-embed") {
        // Clang understands -target arm-none-eabi, but -target riscv-none-embed
        // causes it to crash with an internal error (code 1). -target riscv32
        // does seem to work.
        target = "riscv32";
      } else if (target == "riscv-none-elf") {
        // Clang does not understand -target riscv-none-elf etc
        target = "riscv32";
      } else if (target == "riscv32-unknown-elf") {
        // In SiFive toolchains,  not xPack
        target = "riscv32";
      } else if (target == "riscv64-unknown-elf") {
        // In SiFive toolchains,  not xPack
      } else if (target == "pic30-elf") {
        return;
      }
      // Clang accepts either '-target foo' or '--target=foo'
      add_flag("-target");
      add_flag(target);
    }

    // Add a flag to set pointer size. Pointer size will be passed to the source
    // analyzer as -m option. This determines some built-in sizes in Clang.
    // If these built-in sizes are wrong, Clang produces errors. One
    // example: if size_t does not match the built-in size, redefinitions of
    // operator new generate errors. The default in Clang seems to be 32
    // bit, so the -m option is essential for 16 bit processors such as
    // ATmega328 (Arduino Uno).
    void add_pointer_size_flag()
    {
      trace_nest("add pointer size flag");
      for (auto const &[header, value]: macros) {
        if (header == "__SIZEOF_POINTER__") {
          trace(header << " = " << value);
          size_t size = 0;
          auto result = std::from_chars(
            value.data(), value.data() + value.size(), size
          );
          if (size && result.ptr == value.data() + value.size()) {
            std::ostringstream flag;
            flag << "-m" << (size*8);
            add_flag(flag.str());
          }
          break;
        }
      }
    }

    void add_macro_definition(std::string_view header, std::string_view value)
    {
      std::string_view name = header.substr(0, header.find('('));
      //
      // Do not undefine or redefine __has_include or __has_include_next, as
      // these have a special meaning in gcc (see
      // https://gcc.gnu.org/onlinedocs/cpp/_005f_005fhas_005finclude.html).
      if (name != "__has_include" && name != "__has_include_next") {
        {
          // Insert -U before -D to avoid warning.  
          std::ostringstream undef;
          undef << "-U" << name;
          add_flag(undef.str());
        }
        std::ostringstream def;
        def << "-D" << header;
        if (!value.empty()) {
          def << "=" << value;
        }
#if 1
        else {
          def << "=";
        }
#endif
        add_flag(def.str());
      }
    }

    void add_flag(std::string_view flag)
    {
      results.builtin_flags.emplace_back(flag);
    }
  };

  // Cached results
  class ThreadSafeResults: public Lockable
  {
    sa::CompilerResults results;
    bool ready;
    
  public:
    sa::CompilerResults const *get(CompilerKey const &key)
    {
      Lock lock(this);
      trace_nest("get compiler results for " << key.compiler);
      if (!ready) {
        ready = true;
        CompilerRunner(key, results).run();
      }
      return &results;
    }
  };
    
  class Cache: public std::map<CompilerKey, ThreadSafeResults>, public Lockable
  {
  };
  
  static Cache cache;
}

sa::CompilerResults const *sa::get_compiler_builtin_flags(
  std::string_view compiler,
  FileKind source_kind,
  std::vector<std::string> const &relevant_flags
)
{
  // TODO: resolve compiler using PATH for efficient caching
  trace_nest("get compiler builtin flags for " << compiler);
  
  CompilerKey key(compiler, source_kind, relevant_flags);
  cache.lock();
  ThreadSafeResults &results = cache[key];
  // We never remove results from the cache, so results address will remain
  // valid. Unlock cache now, to allow other threads to access it. The results
  // contents have their own lock.
  cache.unlock();
  return results.get(key);
}

#ifdef SELFTEST

#include <iostream>
#include <algorithm>
#include <tuple>
#include "base/debug.h"

static std::string_view find_flag(
  std::string_view prefix,   std::vector<std::string> const &flags
)
{
  for (std::string_view flag: flags) {
    if (flag.substr(0, prefix.size()) == prefix) {
      return flag;
    }
  }
  return "";
}

static std::vector<std::string> filter(std::vector<std::string> const &flags)
{
  std::vector<std::string> relevant_flags;
  for (auto const &flag: flags) {
    if (sa::is_compiler_builtin_relevant(flag)) {
      relevant_flags.push_back(flag);
    }
  }
  return relevant_flags;
}

static void check_compiler_success(
  std::string_view compiler,
  std::string_view source,
  std::vector<std::string> const &user_flags,
  std::string_view expected_bits_flag,
  std::vector<sa::CompilerDiagnostic> const &expected_diagnostics
)
{
  trace_nest("Check success for " << compiler << " " << source);
  sa::CompilerResults const *result = sa::get_compiler_builtin_flags(
    compiler, source, filter(user_flags)
  );
  assert(result->valid);
  std::cout << "Builtin flags for " << compiler << ": "
            << result->builtin_flags.size() << "\n";
  for (auto const &flag: result->builtin_flags) {
    //trace("flag: " << flag);
    (void)flag;
  }
  assert(!result->builtin_flags.empty());
  auto bits_flag = find_flag("-m", result->builtin_flags);
  std::cout << "bits flag: " << bits_flag << "\n";
  assert(bits_flag == expected_bits_flag);
  assert(result->diagnostics == expected_diagnostics);
}

static void check_compiler_failure(
  std::string_view compiler,
  std::string_view source,
  std::vector<std::string> const &user_flags,
  std::vector<sa::CompilerDiagnostic> expected_diagnostics
)
{
  trace_nest("Check failure for " << compiler << " " << source);
  sa::CompilerResults const *result = sa::get_compiler_builtin_flags(
    compiler, source, filter(user_flags)
  );
  assert(!result->valid);
  for (auto &diagnostic: expected_diagnostics) {
    diagnostic.message = diagnostic.message.substr(0,20);
  }
  std::vector<sa::CompilerDiagnostic> result_diagnostics;
  for (auto const &diagnostic: result->diagnostics) {
    result_diagnostics.emplace_back(diagnostic);
    result_diagnostics.back().message =
      result_diagnostics.back().message.substr(0,20);
  }
  if (result_diagnostics != expected_diagnostics) {
    for (auto const &[message, severity, category]: expected_diagnostics) {
      std::cout << "Expected: " << severity << ": [" << category << "] '"
                << message << "' (" << message.size() << ")\n";
    }
    for (auto const &[message, severity, category]: result_diagnostics) {
      std::cout << "Obtained: " << severity << ": [" << category << "] '"
                << message << "' (" << message.size() << ")\n";
    }
    std::string m1 = expected_diagnostics.front().message;
    std::string m2 = result_diagnostics.front().message;
    for (size_t i = 0; i < m1.size(); ++i) {
      trace(i << " '" << m1[i] << "' '" << (i < m2.size() ? m2[i] : '?'));
    }
  }
  assert(result_diagnostics == expected_diagnostics);
}
  
int main(int argc, const char *argv[])
{
  std::cout << "Hello\n";
  check_compiler_success("gcc", "foo.c", {}, "-m64", {});

  check_compiler_failure("gcc", "foo.c", {"-foox=y"},
    {
     {"unrecognized command line option '-foox=y' (gcc)",
      sa::Severity_fatal, sa::Category_makefile
     },
    }
  );
  assert(argc > 1);
  std::string arm = base::get_absolute_path(
    argv[1], base::get_working_directory()
  ) + "/bin/arm-none-eabi-gcc" + base::os::get_exe_extension();
  std::cout << "arm compiler: " << arm << "\n";
  const char *arm_compiler = arm.data();
  assert_(base::is_file(arm), "no arm compiler found");
  check_compiler_success(arm_compiler, "foo.c", {}, "-m32", {});

  check_compiler_failure(
    "/home/johan/beetle_games/linux/gnu_arm_toolchain_10.3.1_20210824_64b"
    "/bin/arm-none-eabi-gcc", "foo.c", {}, {
     {"cannot execute "
      "/home/johan/beetle_games/linux/gnu_arm_toolchain_10.3.1_20210824_64b"
      "/bin/arm-none-eabi-gcc: No such file or directory",
      sa::Severity_fatal, sa::Category_toolchain
     },
    }
  );

  std::cout << "Selftest succeeded\n";
  return 0;
}

#endif

