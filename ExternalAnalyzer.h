// Copyright 2018-2024 Johan Cockx
#ifndef __ExternalAnalyzer_h
#define __ExternalAnalyzer_h

#include "Analyzer.h"
#include "EntityKind.h"
#include "OccurrenceKind.h"
#include "Severity.h"
#include "File.h"
#include "FileLocation.h"
#include "Symbol.h"
#include "Range.h"
#include "Location.h"

#include "base/ptr.h"
#include <string>

namespace sa {
  class LineOffsetTable;
  //
  // An external analyzer executes an external command to analyze a source file.
  //
  // The external command is supposed to write information about symbol
  // occurrences, diagnostics and memory regions found in the source file to
  // standard output in a specific easy-to-parse format described below. It can
  // also write diagnostics to standard error.
  //
  // The external analyzer will parse the external command's output and report
  // symbols, occurrences and diagnostics found by calling the virtual methods
  // declared below.
  //
  // Output format
  // -------------
  //
  // Each statement begins on a new line.  The first character indicates the
  // kind of statement.
  //
  // Most statements continue with the location of a symbol occurrence, followed
  // by a single space and the symbol's name, followed by an end-of-line marker
  // '\n'. A symbol name can containing any character except for '\n'.
  //
  // The location's source file is implicit and is given by the most recent
  // preceding '@' statement. The location within the source file can be:
  //
  //    - empty, indicating missing source reference information;
  //
  //    - a single integer offset: the zero-based byte offset of the start of
  //      the symbol from the start of the file; or
  //
  //    - two integers separated by a colon line:column, with a zero-based line
  //      and column number.
  //
  // An '@' statement marks a source file as used and sets the source file for
  // all source references in the following statements. The path starts
  // immediately after the '@' and continues up to the end of the line.  A
  // source file path can contain any character except for '\n'.
  //
  // The 'i' statement represents the inclusion of another file. The "symbol
  // name" in this case is the name of the included file as written in the
  // source code, and the location gives the position of the first character of
  // that name in the source code. Every 'i' statement is followed by the path
  // of an hdir on a separate line. The hdir can be an empty string.  If the
  // included file's path is relative, it is relative to the hdir.
  //
  // The 'L' statement represents the address range of a memory region. It does
  // not have a source location. It consists of two decimal numbers and a memory
  // region name, separated by single spaces. The numbers are the origin and
  // size of the memory region.
  //
  // Paths do not need to be normalized: they can contains consecutive slashes,
  // trailing slashes, '/foo/../' and '/./' sequences etcetera. A relative path
  // is relative to the build directory unless otherwise noted.
  //
  // These are the available statements:
  //
  // '@': Mark source file as used and set source file for all following
  //      occurrences.  Set to empty string if following occurrences do not have
  //      a file location.
  //
  // 'L': Memory region; followed by origin space size space name.
  //
  // 'S': Memory section; followed by location, name, runtime region and load
  //      region.
  //
  // '^': Entry point, currently treated like 'u'
  //
  // 'B': Non-UTF-8 character detected in current source files, followed by
  //      location (offset or line:column)
  //
  // 'i': Include symbol as file; symbol is the included file as written in the
  //      source code. Hdir follows on next line.  The following line is most
  //      probably an '@' statement with the actual path of the included file.
  //
  // 'E': Error message, preceded by error location
  //
  // 'n': Symbol is a section name
  //
  // 'u': Use symbol, without declaring its kind or linkage. Default: see
  //      further.
  //
  // Definitions and declarations: symbols are global constants of unspecified
  // kind unless defined or declared as function or variable.
  //
  // 'F': Define symbol as function (in a text section).
  //
  // 'D': Define symbol as variable (in a non-text section).
  //
  // 'f': Declare symbol as a function.
  //
  // 'd': Declare symbol as a variable.
  //
  // It is possible to have multiple assignments of the same symbol. If the
  // symbol is used before the first assignment, it refers to the *last*
  // assignment in the script. Otherwise, it refers to the previous assignment,
  // i.e. the last one before the use. This behavior is the consequence of the
  // fixed point algorithm used in the linker.
  //
  // 'P': Provide fallback value (linkerscript)
  //
  // 'H': Assign hidden value (linkerscript)
  //
  // 'R': Provide hidden fallback (linkerscript)
  //
  // 'A': Set symbol. Same as 'a' (see further), except that the symbol is
  //      global by default. Used for assignments in linker scripts.
  //
  // Declare a symbol as local, weak, common or strong. Unless declared
  // otherwise, symbols are local if defined and strong otherwise. Priority of
  // local > weak > common > strong.
  //
  // Symbols remain global constants of unspecified kind unless another
  // statement marks them as function or variable.
  //
  // 'l': Declare a symbol as local (static), e.g. ASM .local  directive.
  //
  // 'w': Declare the symbol as weak, e.g. ASM .weak directive.  For a weak
  //      symbol, both definitions and uses in the current unit are weak.
  //
  // 'c': Declare the symbol as common, e.g. ASM .common directive.
  //
  // 'g': Declare the symbol as strong, e.g. ASM .global directive. This only
  //      has an effect for defined symbols, because undefined symbols are
  //      strong by default.
  //
  // 'e': Declare the symbol as strong if not defined.  This is a no-op, because
  //      undefined symbols are strong by default.
  //
  // 'a': Set symbol; for example an assignment in a linker script or a .set or
  //      .equ or plain foo = ... assembly statement. The symbol is local unless
  //      explicitly marked global elsewhere. The value is absolute, i.e. not
  //      affected by relocations; in nm output, the letter is a if local or A
  //      if global.
  //
  //      This does not allocate memory for the symbol, but defines a link-time
  //      constant. The value will be the address of an extern symbol with the
  //      same name in C/C++.
  //
  //      The value can be overwritten.  Any local use refers to the most recent
  //      assignment before the use. A use before the first assignment is
  //      allowed.  In gcc, it gets the value of the first assignment.  In
  //      llvm/lld, it gets the value of the last assignment.
  //
  //      This is similar to but independent of 'l'.  In contrast to 'l',
  //      constants are absolute (not relocatable), have a compile-time value
  //      and can be overwritten by the next statement targetting the same
  //      variable.
  //
  class OriginalExternalAnalyzer {
  public:
    // Create external analyzer.  Initial source file will be used for
    // occurrences and errors in the command line.
    OriginalExternalAnalyzer(base::ptr<File> initial_source_file);
    virtual ~OriginalExternalAnalyzer();
    
    // Run external analysis command. Return the command's exit code.
    // If the command cannot be started, report a fatal error and return -1.
    int run(
      const char *args[],
      const char *work_directory
    );  

  protected:
    base::ptr<File> initial_source_file;

    // Working directory while running, otherwise null.  Used to make file paths
    // absolute.
    const char *work_directory = 0;

    // Prefix relative path with work directory and get file.
    base::ptr<File> get_work_file(const std::string &path);
    
    void handle_stdout(std::istream &in);

    size_t get_offset(base::ptr<File> file, unsigned line, unsigned column);

    // Stderr must be read, even if it is not used.  Otherwise, the external
    // process might block when I/O buffers are full.
    virtual void handle_stderr(std::istream &in) = 0;

    // Get file. Path must be either absolute or relative to the build path.
    virtual base::ptr<File> get_file(const std::string &path) = 0;

    virtual Section *create_section(
      const std::string &name,
      const std::string &member_name = ""
    ) = 0;

    virtual base::ptr<Symbol> get_global_symbol(
      const std::string &link_name
    ) = 0;

    virtual base::ptr<Symbol> get_local_symbol(
      EntityKind kind,
      const std::string &user_name,
      const FileLocation &ref_location
    ) = 0;
    
    // Add a file inclusion occurrence. If hdir path is not empty, it must be
    // either absolute or relative to the build path.
    virtual void add_include(
      base::ptr<File> const &included_file,
      base::ptr<File> const &including_file,
      Range range,
      const std::string &hdir_path = ""
    )
    {
      (void)included_file;
      (void)including_file;
      (void)range;
      (void)hdir_path;
    }

    // Add a symbol occurrence.
    virtual Occurrence *add_occurrence(
      base::ptr<Symbol> const &symbol,
      EntityKind ekind,
      OccurrenceKind okind,
      Section *section,
      base::ptr<File> const &file,
      Range range
    ) = 0;
    virtual Occurrence *add_occurrence(
      base::ptr<Symbol> const &symbol,
      OccurrenceKind kind,
      OccurrenceStyle style,
      Section *section,
      base::ptr<File> const &file,
      Range range
    ) = 0;

    virtual void open_scope() {}
    
    virtual void close_scope() {}

    virtual void report_diagnostic(
      const std::string &message,
      Severity severity,
      base::ptr<File> const &file,
      Location location
    )
    {
      (void)message;
      (void)severity;
      (void)file;
      (void)location;
    }

    virtual void report_missing_header(const std::string &name)
    {
      (void)name;
    }

    virtual void add_memory_region(
      const std::string &name,
      size_t origin,
      size_t size
    )
    {
      (void)name;
      (void)origin;
      (void)size;
    }

    virtual void add_memory_section(
      const std::string &name,
      const std::string &runtime_region,
      const std::string &load_region
    )
    {
      (void)name;
      (void)runtime_region;
      (void)load_region;
    }

    // Report the presence of at least one non-UTF-8 character in a file.
    // Additional calls for the same file have no effect.
    virtual void report_non_utf8_file(base::ptr<File> const &file)
    {
      (void)file;
    }

    std::map<base::ptr<File>, LineOffsetTable*> table_map;
    bool valid = false;
  };
  
  class ExternalAnalyzer: public Analyzer {
  public:
    // Create external analyzer.  Initial source file will be used for
    // occurrences and errors in the command line.
    ExternalAnalyzer(Unit *unit);
    virtual ~ExternalAnalyzer();
    
    // Run external analysis command. Return the command's exit code.
    // If the command cannot be started, report a fatal error and return -1.
    int run_external_command(
      const char *args[],
      const char *work_directory
    );  

  protected:
    base::ptr<File> initial_source_file;

    // Working directory while running, otherwise null.  Used to make file paths
    // absolute.
    const char *work_directory = 0;

    // Prefix relative path with work directory and get file.
    base::ptr<File> get_work_file(const std::string &path);
    
    void handle_stdout(std::istream &in);

    size_t get_offset(base::ptr<File> file, unsigned line, unsigned column);

    // Stderr must be read, even if it is not used.  Otherwise, the external
    // process might block when I/O buffers are full.
    virtual void handle_stderr(std::istream &in) = 0;

    virtual bool is_linker_relevant_diagnostic(
      sa::Severity severity,
      const std::string &message
    )
    {
      (void)message;
      return severity >= Severity_error;
    }
    
    std::map<base::ptr<File>, LineOffsetTable*> table_map;
    bool valid = false;
  };
}

#endif
