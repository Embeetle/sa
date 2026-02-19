// Copyright 2018-2024 Johan Cockx
#include "ExternalAnalyzer.h"
#include "LineOffsetTable.h"
#include "environment.h"
#include "base/os.h"
#include "base/filesystem.h"
#include "base/print.h"

sa::OriginalExternalAnalyzer::OriginalExternalAnalyzer(base::ptr<File> initial_source_file)
  : initial_source_file(initial_source_file)
{
  assert(base::is_valid(initial_source_file));
}

sa::OriginalExternalAnalyzer::~OriginalExternalAnalyzer()
{
  for (auto &[file, table]: table_map) {
    delete table;
  }
}

int sa::OriginalExternalAnalyzer::run(
  const char *args[],
  const char *work_directory
)
{
  trace_atomic_code(
    std::cerr << "\nexternal analyzer command:\n cd " << work_directory << "\n";
    for (unsigned i = 0; args[i]; ++i) {
      std::cerr << " '" << args[i] << "'";
    }
    std::cerr << "\n\n";
  );
  assert(!this->work_directory);
  this->work_directory = work_directory;
  int exit_code = -1; // Will be overwritten.
  int error_number = base::os::execute_and_capture(
    args, work_directory, this,
    &OriginalExternalAnalyzer::handle_stdout,
    &OriginalExternalAnalyzer::handle_stderr,
    exit_code, standard_environment
  );
  trace("exit code: " << exit_code);
  if (error_number) {
    report_diagnostic(
      std::string() + "internal error: " + base::os::strerror(error_number)
      + " while trying to execute " + base::quote_command_line(args)
      , Severity_fatal, 0, Location()
    );
  }
  this->work_directory = 0;
  return exit_code;
}

base::ptr<sa::File> sa::OriginalExternalAnalyzer::get_work_file(const std::string &path)
{
  assert(work_directory);
  return get_file(base::join_paths(work_directory, path));
}

void sa::OriginalExternalAnalyzer::handle_stdout(std::istream &in)
{
  trace_nest("processing external stdout");
  
  struct OccurrenceData {
    base::ptr<File> file;
    unsigned offset;
    OccurrenceKind kind;  // Only definition, declaration or use, refined later
    OccurrenceData(
      OccurrenceKind kind,
      base::ptr<File> file,
      unsigned offset
    ): file(file), offset(offset), kind(kind)
    {
    }
  };
  
  struct SymbolData {
    // Definition available in this unit?
    bool is_defined = false;

    // Linkage flags, highest priority first
    //
    // In gcc assembly, a .weak declaration has priority over a .global (strong)
    // declaration. A combination of .weak and .common triggers a diagnostic.
    // I did not check what a combination of .common with .global yields; assume
    // .common has priority over .global.
    bool is_local = false;
    bool is_weak = false;
    bool is_common = false;
    bool is_strong = false;

    // Kind of symbol. If none or both are set,  symbol is a global constant
    // of unspecified kind.
    bool is_function = false;
    bool is_variable = false;

    // Occurrences of this symbol.
    std::vector<OccurrenceData> occurrences;

    // Add an occurrence of this symbol.
    void add(
      OccurrenceKind kind,
      base::ptr<File> file,
      unsigned offset
    )
    {
      occurrences.emplace_back(kind, file, offset);
    }

    // Insert this symbol in the analyzer.
    void insert_symbol(
      const std::string &name,
      OriginalExternalAnalyzer *analyzer,
      Section *section
    )
    {
      trace_nest("insert symbol " << name);
      // Each defined symbol should also have occurrences, unless there is a bug
      // in the parsing code or the external analyzer. When there are no
      // occurrences, the symbol will be automatically deleted after insertion,
      // but the project is not locked at that point, so an assertion will fail.
      assert(occurrences.size());
      // Undefined symbols in assembly code are always global symbols. In
      // addition to undefined symbols, globals include strong, weak and common
      // definitions.
      bool is_global = !is_local && (
        !is_defined || is_strong || is_common || is_weak
      );
      EntityKind ekind =
        //
        // Function
        is_function && !is_variable
        ? (is_global ? EntityKind_global_function : EntityKind_local_function)
        //
        // Variable
        : !is_function && is_variable
        ? (is_global
          ? EntityKind_global_variable
          : EntityKind_local_static_variable
        )
        // Neither function or variable,  or both function and variable
        : (is_global ? EntityKind_global_symbol : EntityKind_local_symbol)
        ;
      base::ptr<Symbol> symbol;
      if (is_global) {
        symbol = analyzer->get_global_symbol(name);
      } else {
        const OccurrenceData &ref = occurrences.at(0);
        FileLocation location;
        location.file = ref.file;
        location.offset = ref.offset;
        symbol = analyzer->get_local_symbol(ekind, name, location);
      }
      unsigned range_size = name.length();
      trace("insert symbol " << name << " (" << occurrences.size() << " occs)");
      for (OccurrenceData &data: occurrences) {
        base::ptr<File> file = data.file;
        OccurrenceKind okind = data.kind;
        if (is_local) {
          // No occurrence kind tweaks required
        } else if (is_weak) {
          if (okind == OccurrenceKind_definition) {
            okind = OccurrenceKind_weak_definition;
          } else if (okind == OccurrenceKind_declaration) {
            okind = OccurrenceKind_weak_declaration;
          } else {
            assert(okind == OccurrenceKind_use);
            okind = OccurrenceKind_weak_use;
          }
        } else if (is_common) {
          if (okind == OccurrenceKind_definition) {
            okind = OccurrenceKind_tentative_definition;
          }
        }
        Range range(data.offset, data.offset + range_size);
        trace("Add " << *symbol << " " << okind << " at " << *file<<" "<<range);
        analyzer->add_occurrence(symbol, ekind, okind, section, file, range);
      }
    }

    // Insert a local constant definition that is about to be overwritten by a
    // new definition (using a .equ directive or similar). Consume existing
    // definition and uses, but keep declarations.
    void insert_local(
      const std::string &name,
      OriginalExternalAnalyzer *analyzer,
      Section *section
    )
    {
      EntityKind kind = EntityKind_local_symbol;
      FileLocation location;
      for (const OccurrenceData &data: occurrences) {
        if (data.kind == OccurrenceKind_definition) {
          location.file = data.file;
          location.offset = data.offset;
          break;
        }
      }
      assert(location.file);
      base::ptr<Symbol> symbol = analyzer->get_local_symbol(
        kind, name, location
      );
      unsigned range_size = name.length();
      std::vector<OccurrenceData> declarations;
      for (const OccurrenceData &data: occurrences) {
        if (data.kind == OccurrenceKind_declaration) {
          declarations.push_back(data);
        } else {
          base::ptr<File> file = data.file;
          OccurrenceKind kind = data.kind;
          Range range(data.offset, data.offset + range_size);
          analyzer->add_occurrence(
            symbol, kind, OccurrenceStyle_unspecified, section,
            file, range
          );
        }
      }
      std::swap(declarations, occurrences);
    }
  };
  
  std::map<std::string, SymbolData> symbol_map;
  std::map<base::ptr<File>, LineOffsetTable*> table_map;
  base::ptr<File> file = initial_source_file;
  LineOffsetTable *table = 0;
  Section *section = create_section("");

  char code;
  std::string name;
  while (in >> code) {
    // ------------------------------------------------------------------------
    // For overview and meaning of codes,  see llvm/llvm/include/Embeetle.h.
    // ------------------------------------------------------------------------
    trace("handle code '" << code << "' peek='" << (char)(in.peek()) << "'");
    if (code == '@') {
      std::string path;
      std::getline(in, path);
      file = get_work_file(path);
      table = 0;
      continue;
    }
    if (code == 'L') {
      size_t origin, size;
      in >> origin >> size;
      in.ignore(); // skip space
      std::getline(in, name);
      if (!in) {
        trace("syntax error in 'L'");
        break;
      }
      trace("region " << (void*)origin << " " << (void*)size << " " << name);
      add_memory_region(name, origin, size);
      continue;
    }
    size_t offset = 0;
    if (in.peek() != ' ') {
      in >> offset;
      if (in.peek() == ':') {
        in.get();
        unsigned line = offset;
        unsigned column = 0;
        in >> column;
        if (!table) {
          auto it = table_map.find(file);
          if (it == table_map.end()) {
            if (file) {
              std::ifstream stream(file->get_path().data());
              table = new LineOffsetTable(stream);
              table_map[file] = table;
            } else {
              std::ifstream stream;
              table = new LineOffsetTable(stream);
            }
          } else {
            table = it->second;
          }
        }
        offset = table->offset_or_zero(line, column);
        if (offset == 0) {
          trace("zero offset for " << line << ":" << column << " in " <<
            (file ? file->get_path() : "")
          );
        }
      }
    }
    if (code == 'B') {
      // Non-UTF-8 characters detected
      report_non_utf8_file(file); // Ignore offset
      continue;
    }
    assert_(in.peek() == ' ', code << offset << " '" << in.peek() << "'");
    in.get(); // Skip space
    if (code == 'i') {
      std::string target;
      std::getline(in, target);
      std::string hdir;
      std::getline(in, hdir);
      trace("got include at " << offset << " of " << target << " from " << hdir);
      // TODO: use project path below?
      std::string included_path = base::is_absolute_path(target) ? target :
        base::join_paths(
          hdir.empty() ? base::get_parent_path(file->get_path()) : hdir, target
        );
      auto included_file = get_work_file(included_path);
      unsigned size = target.size();
      Range range(offset, offset + size);
      add_include(included_file, file, range, hdir);
      continue;
    }
    std::getline(in, name);
    trace("got " << code << offset << " " << name);
    if (name.empty()) {
      trace("External analyzer syntax error: code=" << code);
      return;
    }
    if (code == 'E') {
      trace("got diagnostic " << name);
      report_diagnostic(name, Severity_error, file, offset);
      continue;
    }
    if (code == 'S') {
      trace("got memory section " << name);
      std::string runtime_region;
      std::getline(in, runtime_region);
      std::string load_region;
      std::getline(in, load_region);
      add_memory_section(name, runtime_region, load_region);
      continue;
    }
    SymbolData &symbol = symbol_map[name];
    switch (code) {
      case 'n':
        // section name
        symbol.is_defined = true; // sections are implicitly defined
        symbol.add(OccurrenceKind_use, file, offset);
        break;
      case '^':
        // Entry point, currently treated as use -> falltru
      case 'u':
        // Use
        symbol.add(OccurrenceKind_use, file, offset);
        break;
      case 'F':
        // Definition in text section
        symbol.is_defined = true;
        symbol.is_function = true;
        symbol.add(OccurrenceKind_definition, file, offset);
        break;
      case 'D':
        // Definition in data section
        symbol.is_defined = true;
        symbol.is_variable = true;
        symbol.add(OccurrenceKind_definition, file, offset);
        break;
      case 'f':
        // .type foo, %function  or similar
        symbol.is_function = true;
        symbol.add(OccurrenceKind_declaration, file, offset);
        break;
      case 'd':
        // .type foo, %object    or similar
        symbol.is_variable = true;
        symbol.add(OccurrenceKind_declaration, file, offset);
        break;
      case 'P': // Linkerscript PROVIDE(sym = ...)
      case 'R': // Linkerscript PROVIDE_HIDDEN(sym = ...)
        // HIDDEN affects visibility of the symbol in the linker output, but not
        // during linking.
        //
        // Is PROVIDE equivalent to weak, or is it even weaker than weak?  This
        // is hard to test: if both are equivalent, they are both effectively
        // weak so the linker can freely choose either one.  In our tests, it
        // happens to choose the symbol declared with __attribute__((weak)), not
        // the value declared with PROVIDE, which proves nothing.
        symbol.is_weak = true;
        symbol.add(OccurrenceKind_definition, file, offset);
        break;
      case 'A': // Linkerscript sym = ...
      case 'H': // Linkerscript HIDDEN(sym = ...)
        // HIDDEN affects visibility of the symbol in the linker output, but not
        // during linking.
        symbol.add(OccurrenceKind_definition, file, offset);
        break;
      case 'l':
        // .local directive
        symbol.is_local = true;
        symbol.add(OccurrenceKind_declaration, file, offset);
        break;
      case 'w':
        // Assembly .weak directive
        symbol.is_weak = true;
        symbol.add(OccurrenceKind_declaration, file, offset);
        break;
      case 'W':
        // Weak use, e.g. in linkerscript DEFINED(foo) expression
        symbol.add(OccurrenceKind_weak_use, file, offset);
        break;
      case 'c':
        // Assembly .common directive
        symbol.is_common = true;
        symbol.add(OccurrenceKind_declaration, file, offset);
        break;
      case 'g':
        // Assembly .global directive
        symbol.is_strong = true;
        symbol.add(OccurrenceKind_declaration, file, offset);
        break;
      case 'e':
        // Assembly .extern directive
        symbol.add(OccurrenceKind_declaration, file, offset);
        break;
      case 'a':
        // Assembly .equ or similar directive
        if (symbol.is_defined) {
          // This definition replaces a previous definition. Generate the
          // previous definition and its uses as a local constant. Ignore
          // global, common and weak declarations.
          symbol.insert_local(name, this, section);
        } else {
          symbol.is_defined = true;
        }
        symbol.add(OccurrenceKind_definition, file, offset);
        break;
      default:
        trace("ASM analyzer unrecognized code " << code);
        break;
    }
  }
  for (auto &[name, symbol]: symbol_map) {
    symbol.insert_symbol(name, this, section);
  }
  for (auto &[file, table]: table_map) {
    delete table;
  }
}

size_t sa::OriginalExternalAnalyzer::get_offset(
  base::ptr<File> file,
  unsigned line,
  unsigned column
)
{
  LineOffsetTable *table;
  auto it = table_map.find(file);
  if (it == table_map.end()) {
    std::ifstream stream(file->get_path().data());
    table = new LineOffsetTable(stream);
    table_map[file] = table;
  } else {
    table = it->second;
  }
  return table->offset_or_zero(line, column);
}

sa::ExternalAnalyzer::ExternalAnalyzer(Unit *unit)
  : Analyzer(unit)
  , initial_source_file(unit->file)
{
  assert(base::is_valid(initial_source_file));
}

sa::ExternalAnalyzer::~ExternalAnalyzer()
{
  for (auto &[file, table]: table_map) {
    delete table;
  }
}

int sa::ExternalAnalyzer::run_external_command(
  const char *args[],
  const char *work_directory
)
{
  trace_atomic_code(
    std::cerr << "\nexternal analyzer command:\n cd " << work_directory << "\n";
    for (unsigned i = 0; args[i]; ++i) {
      std::cerr << " '" << args[i] << "'";
    }
    std::cerr << "\n\n";
  );
  assert(!this->work_directory);
  this->work_directory = work_directory;
  int exit_code = -1; // Will be overwritten.
  int error_number = base::os::execute_and_capture(
    args, work_directory,
    this, &ExternalAnalyzer::handle_stdout, &ExternalAnalyzer::handle_stderr,
    exit_code, standard_environment
  );
  trace("exit code: " << exit_code);
  if (error_number) {
    report_diagnostic(
      std::string() + "internal error: " + base::os::strerror(error_number)
      + " while trying to execute " + base::quote_command_line(args)
      , Severity_fatal, 0, Location(), true
    );
  }
  this->work_directory = 0;
  return exit_code;
}

base::ptr<sa::File> sa::ExternalAnalyzer::get_work_file(const std::string &path)
{
  assert(work_directory);
  return get_file(base::join_paths(work_directory, path));
}

void sa::ExternalAnalyzer::handle_stdout(std::istream &in)
{
  trace_nest("processing external stdout");
  
  struct OccurrenceData {
    base::ptr<File> file;
    unsigned offset;
    OccurrenceKind kind;  // Only definition, declaration or use, refined later
    OccurrenceData(
      OccurrenceKind kind,
      base::ptr<File> file,
      unsigned offset
    ): file(file), offset(offset), kind(kind)
    {
    }
  };
  
  struct SymbolData {
    // Definition available in this unit?
    bool is_defined = false;

    // Linkage flags, highest priority first
    //
    // In gcc assembly, a .weak declaration has priority over a .global (strong)
    // declaration. A combination of .weak and .common triggers a diagnostic.
    // I did not check what a combination of .common with .global yields; assume
    // .common has priority over .global.
    bool is_local = false;
    bool is_weak = false;
    bool is_common = false;
    bool is_strong = false;

    // Kind of symbol. If none or both are set,  symbol is a global constant
    // of unspecified kind.
    bool is_function = false;
    bool is_variable = false;

    // Occurrences of this symbol.
    std::vector<OccurrenceData> occurrences;

    // Add an occurrence of this symbol.
    void add(
      OccurrenceKind kind,
      base::ptr<File> file,
      unsigned offset
    )
    {
      occurrences.emplace_back(kind, file, offset);
    }

    // Insert this symbol in the analyzer.
    void insert_symbol(
      const std::string &name,
      ExternalAnalyzer *analyzer,
      Section *section
    )
    {
      trace_nest("insert symbol " << name);
      // Each defined symbol should also have occurrences, unless there is a bug
      // in the parsing code or the external analyzer. When there are no
      // occurrences, the symbol will be automatically deleted after insertion,
      // but the project is not locked at that point, so an assertion will fail.
      assert(occurrences.size());
      // Undefined symbols in assembly code are always global symbols. In
      // addition to undefined symbols, globals include strong, weak and common
      // definitions.
      bool is_global = !is_local && (
        !is_defined || is_strong || is_common || is_weak
      );
      EntityKind ekind =
        //
        // Function
        is_function && !is_variable
        ? (is_global ? EntityKind_global_function : EntityKind_local_function)
        //
        // Variable
        : !is_function && is_variable
        ? (is_global
          ? EntityKind_global_variable
          : EntityKind_local_static_variable
        )
        // Neither function or variable,  or both function and variable
        : (is_global ? EntityKind_global_symbol : EntityKind_local_symbol)
        ;
      base::ptr<Symbol> symbol;
      if (is_global) {
        symbol = analyzer->get_global_symbol(name);
      } else {
        const OccurrenceData &ref = occurrences.at(0);
        FileLocation location;
        location.file = ref.file;
        location.offset = ref.offset;
        symbol = analyzer->get_local_symbol(ekind, name, location);
      }
      unsigned range_size = name.length();
      trace("insert symbol " << name << " (" << occurrences.size() << " occs)");
      for (OccurrenceData &data: occurrences) {
        base::ptr<File> file = data.file;
        OccurrenceKind okind = data.kind;
        if (is_local) {
          // No occurrence kind tweaks required
        } else if (is_weak) {
          if (okind == OccurrenceKind_definition) {
            okind = OccurrenceKind_weak_definition;
          } else if (okind == OccurrenceKind_declaration) {
            okind = OccurrenceKind_weak_declaration;
          } else {
            assert(okind == OccurrenceKind_use);
            okind = OccurrenceKind_weak_use;
          }
        } else if (is_common) {
          if (okind == OccurrenceKind_definition) {
            okind = OccurrenceKind_tentative_definition;
          }
        }
        Range range(data.offset, data.offset + range_size);
        trace("Add " << *symbol << " " << okind << " at " << *file<<" "<<range);
        analyzer->add_occurrence(
          symbol, ekind, okind, section, file, range, true
        );
      }
    }

    // Insert a local constant definition that is about to be overwritten by a
    // new definition (using a .equ directive or similar). Consume existing
    // definition and uses, but keep declarations.
    void insert_local(
      const std::string &name,
      ExternalAnalyzer *analyzer,
      Section *section
    )
    {
      EntityKind kind = EntityKind_local_symbol;
      FileLocation location;
      for (const OccurrenceData &data: occurrences) {
        if (data.kind == OccurrenceKind_definition) {
          location.file = data.file;
          location.offset = data.offset;
          break;
        }
      }
      assert(location.file);
      base::ptr<Symbol> symbol = analyzer->get_local_symbol(
        kind, name, location
      );
      unsigned range_size = name.length();
      std::vector<OccurrenceData> declarations;
      for (const OccurrenceData &data: occurrences) {
        if (data.kind == OccurrenceKind_declaration) {
          declarations.push_back(data);
        } else {
          base::ptr<File> file = data.file;
          OccurrenceKind kind = data.kind;
          Range range(data.offset, data.offset + range_size);
          analyzer->add_occurrence(
            symbol, kind, OccurrenceStyle_unspecified, section,
            file, range, true
          );
        }
      }
      std::swap(declarations, occurrences);
    }
  };
  
  std::map<std::string, SymbolData> symbol_map;
  std::map<base::ptr<File>, LineOffsetTable*> table_map;
  base::ptr<File> file = initial_source_file;
  LineOffsetTable *table = 0;
  Section *section = create_section("");

  char code;
  std::string name;
  while (in >> code) {
    // ------------------------------------------------------------------------
    // For overview and meaning of codes,  see llvm/llvm/include/Embeetle.h.
    // ------------------------------------------------------------------------
    trace("handle code '" << code << "' peek='" << (char)(in.peek()) << "'");
    if (code == '@') {
      std::string path;
      std::getline(in, path);
      file = get_work_file(path);
      table = 0;
      continue;
    }
    if (code == 'L') {
      size_t origin, size;
      in >> origin >> size;
      in.ignore(); // skip space
      std::getline(in, name);
      if (!in) {
        trace("syntax error in 'L'");
        break;
      }
      trace("region " << (void*)origin << " " << (void*)size << " " << name);
      add_memory_region(name, origin, size);
      continue;
    }
    size_t offset = 0;
    if (in.peek() != ' ') {
      in >> offset;
      if (in.peek() == ':') {
        in.get();
        unsigned line = offset;
        unsigned column = 0;
        in >> column;
        if (!table) {
          auto it = table_map.find(file);
          if (it == table_map.end()) {
            if (file) {
              std::ifstream stream(file->get_path().data());
              table = new LineOffsetTable(stream);
              table_map[file] = table;
            } else {
              std::ifstream stream;
              table = new LineOffsetTable(stream);
            }
          } else {
            table = it->second;
          }
        }
        offset = table->offset_or_zero(line, column);
        if (offset == 0) {
          trace("zero offset for " << line << ":" << column << " in " <<
            (file ? file->get_path() : "")
          );
        }
      }
    }
    if (code == 'B') {
      // Non-UTF-8 characters detected
      report_non_utf8_file(file); // Ignore offset
      continue;
    }
    assert_(in.peek() == ' ', code << offset << " '" << in.peek() << "'");
    in.get(); // Skip space
    if (code == 'i') {
      std::string target;
      std::getline(in, target);
      std::string hdir;
      std::getline(in, hdir);
      trace("got include at " << offset << " of " << target << " from " << hdir);
      // TODO: use project path below?
      std::string included_path = base::is_absolute_path(target) ? target :
        base::join_paths(
          hdir.empty() ? base::get_parent_path(file->get_path()) : hdir, target
        );
      auto included_file = get_work_file(included_path);
      unsigned size = target.size();
      Range range(offset, offset + size);
      add_include(included_file, file, range, hdir);
      continue;
    }
    std::getline(in, name);
    trace("got " << code << offset << " " << name);
    if (name.empty()) {
      trace("External analyzer syntax error: code=" << code);
      return;
    }
    if (code == 'E') {
      trace("got diagnostic " << name);
      report_diagnostic(name, Severity_error, file, offset,
        is_linker_relevant_diagnostic(Severity_error, name)
      );
      continue;
    }
    if (code == 'S') {
      trace("got memory section " << name);
      std::string runtime_region;
      std::getline(in, runtime_region);
      std::string load_region;
      std::getline(in, load_region);
      add_memory_section(name, runtime_region, load_region);
      continue;
    }
    SymbolData &symbol = symbol_map[name];
    switch (code) {
      case 'n':
        // section name
        symbol.is_defined = true; // sections are implicitly defined
        symbol.add(OccurrenceKind_use, file, offset);
        break;
      case '^':
        // Entry point, currently treated as use -> falltru
      case 'u':
        // Use
        symbol.add(OccurrenceKind_use, file, offset);
        break;
      case 'F':
        // Definition in text section
        symbol.is_defined = true;
        symbol.is_function = true;
        symbol.add(OccurrenceKind_definition, file, offset);
        break;
      case 'D':
        // Definition in data section
        symbol.is_defined = true;
        symbol.is_variable = true;
        symbol.add(OccurrenceKind_definition, file, offset);
        break;
      case 'f':
        // .type foo, %function  or similar
        symbol.is_function = true;
        symbol.add(OccurrenceKind_declaration, file, offset);
        break;
      case 'd':
        // .type foo, %object    or similar
        symbol.is_variable = true;
        symbol.add(OccurrenceKind_declaration, file, offset);
        break;
      case 'P': // Linkerscript PROVIDE(sym = ...)
      case 'R': // Linkerscript PROVIDE_HIDDEN(sym = ...)
        // HIDDEN affects visibility of the symbol in the linker output, but not
        // during linking.
        //
        // Is PROVIDE equivalent to weak, or is it even weaker than weak?  This
        // is hard to test: if both are equivalent, they are both effectively
        // weak so the linker can freely choose either one.  In our tests, it
        // happens to choose the symbol declared with __attribute__((weak)), not
        // the value declared with PROVIDE, which proves nothing.
        symbol.is_weak = true;
        symbol.add(OccurrenceKind_definition, file, offset);
        break;
      case 'A': // Linkerscript sym = ...
      case 'H': // Linkerscript HIDDEN(sym = ...)
        // HIDDEN affects visibility of the symbol in the linker output, but not
        // during linking.
        symbol.add(OccurrenceKind_definition, file, offset);
        break;
      case 'l':
        // .local directive
        symbol.is_local = true;
        symbol.add(OccurrenceKind_declaration, file, offset);
        break;
      case 'w':
        // Assembly .weak directive
        symbol.is_weak = true;
        symbol.add(OccurrenceKind_declaration, file, offset);
        break;
      case 'W':
        // Weak use, e.g. in linkerscript DEFINED(foo) expression
        symbol.add(OccurrenceKind_weak_use, file, offset);
        break;
      case 'c':
        // Assembly .common directive
        symbol.is_common = true;
        symbol.add(OccurrenceKind_declaration, file, offset);
        break;
      case 'g':
        // Assembly .global directive
        symbol.is_strong = true;
        symbol.add(OccurrenceKind_declaration, file, offset);
        break;
      case 'e':
        // Assembly .extern directive
        symbol.add(OccurrenceKind_declaration, file, offset);
        break;
      case 'a':
        // Assembly .equ or similar directive
        if (symbol.is_defined) {
          // This definition replaces a previous definition. Generate the
          // previous definition and its uses as a local constant. Ignore
          // global, common and weak declarations.
          symbol.insert_local(name, this, section);
        } else {
          symbol.is_defined = true;
        }
        symbol.add(OccurrenceKind_definition, file, offset);
        break;
      default:
        trace("ASM analyzer unrecognized code " << code);
        break;
    }
  }
  for (auto &[name, symbol]: symbol_map) {
    symbol.insert_symbol(name, this, section);
  }
  for (auto &[file, table]: table_map) {
    delete table;
  }
}

size_t sa::ExternalAnalyzer::get_offset(
  base::ptr<File> file,
  unsigned line,
  unsigned column
)
{
  LineOffsetTable *table;
  auto it = table_map.find(file);
  if (it == table_map.end()) {
    std::ifstream stream(file->get_path().data());
    table = new LineOffsetTable(stream);
    table_map[file] = table;
  } else {
    table = it->second;
  }
  return table->offset_or_zero(line, column);
}
