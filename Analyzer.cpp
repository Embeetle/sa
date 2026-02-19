// Copyright 2018-2024 Johan Cockx
#include "Analyzer.h"
#include "GlobalSymbol.h"
#include "LocalSymbol.h"
#include "Project.h"
#include "File.h"
#include "Hdir.h"
#include "Unit.h"
#include "Inclusion.h"
#include "EmptyLoop.h"
#include "base/filesystem.h"
#include "base/Timer.h"
#include <memory>

// Analysis model
// ==============
//
// Source file are text files in some defined format (C/C++/Assembly) that may
// include header files. A compilation unit is a source file plus all included
// header files. Compilation units are analyzed with specific compilation
// options (such as preprocessor defines and include directories) that are
// extracted from the makefile.
//
// Source files contain occurrences of symbols at specific offsets (bytes into
// the file - not compilation unit - of the first byte of the occurrence) and
// sizes (number of bytes in the occurrence).
//
// Symbols can be functions, variables, constants, types, fields, macros, and
// any other symbols that only have a meaning within the compilation
// unit. Symbols can come in different varieties.  One specific variety of
// functions, variables and constants are global symbols. There are global
// functions, global variables and global constants; all other symbols are local
// to the compilation unit.
//
// Global symbols are special because their occurrences are merged over
// compilation units by a linker, based on their name.  Local symbols are never
// merged between compilation units; they are not touched by a linker.
// Regardless, local symbols that occur in header files are treated from
// a user point-of-view as a single symbol that happens to have a distinct
// instantiations in multiple compilation units.
//
// Global symbols have a unique link name.  Local symbols are identified by
// their name plus the file and offset of their first occurrence. This way,
// local symbols declared or defined in a header file that is included in
// multiple compilation units can be treated as a single symbol, while local
// symbols with the same name in the same compilation unit can be treated as
// distinct, taking into account the semantics of the language of their
// compilation unit.
//
// Global symbols have a value, which is an address for functions and variables
// and a non-address value for constants. Functions and variables have a section
// and can also have a size.
//
// Occurrences can be definitions, declarations and uses.  Definitions can be
// strong (=default), common or weak. If a symbol is used, it must have at least
// one definition, or the symbol is undefined (=link error).
//
// If there are definitions,  they are combined as follows:
//
// - If there is more than one strong definition, the symbol is multiply defined
//   (=link error).
//
// - Otherwise, if there is exactly one strong definition, then this defines the
//   value, size and section for all common definitions and uses.  Weak
//   definitions are ignored.
//
// - Otherwise, if there are one or more common definitions, the common
//   definitions are relocated to the same address which defines the value, and
//   the size is the maximum of the sizes of the common definitions. The section
//   is decided by the linker script: normally the bss section (zero-initialized
//   data) is used.  Weak definitions are ignored.
//
// - Otherwise, if there is at least one weak definition, an arbitrary weak
//   definition defines the value, size and section, and the other weak
//   definitions are discarded.
//
// Common variables can be generated from C code using 'int foo;' (without
// initialization and without 'extern') at the top level, and compiling with
// -fcommon. This exists mainly to support ancient C code.
//
// Refer to 'ld' documentation for more information.


static base::Timer overall_timer("Overall", false);
static base::Timer prelude_timer("Prelude", false);
static base::Timer report_timer("Report");
static base::Timer cache_read_timer("Cache read");
static base::Timer cache_file_read_timer("Cache file read");
static base::Timer cache_write_timer("Cache write");
static base::Timer analysis_timer("Clang analysis");

static void print_flag(const std::string &flag_buffer, unsigned index)
{
  (void)print_flag;
  const char *flag = flag_buffer.data();
  for (unsigned i = 0; i < index; i++) {
    flag += strlen(flag) + 1;
  }
  std::cerr << "flag " << index << " is " << flag << std::endl;
}

static void print_flags(const std::string &flag_buffer)
{
  (void)print_flags;
  const char *flag = flag_buffer.data();
  const char *end = flag + flag_buffer.size();
  for (; flag < end; flag += strlen(flag) + 1) {
    debug_stream << flag << " ";
  }
  debug_stream << "\n";
}

static void print_includes(const std::string &flag_buffer)
{
  (void)print_includes;
  const char *flag = flag_buffer.data();
  const char *end = flag + flag_buffer.size();
  bool force_print = false;
  debug_atomic_code(
    for (; flag < end; flag += strlen(flag) + 1) {
      if (flag[0] == '-' && flag[1] == 'I') {
        debug_stream << "\n";
        debug_write_prefix() << "flag: " << flag;
        force_print = !flag[2];
      } else if (force_print) {
        force_print = false;
        debug_stream << " " << flag;
      }
    }
    debug_stream << "\n";
  )    
}

sa::Analyzer::Analyzer(Unit *unit)
  : unit(unit)
  , project(unit->file->project)
  , cache_path(
    project->get_cache_path() + "/" + unit->process_name() + ".cache"
  )
{
  trace("Create analyzer " << unit->process_name());
  parent_stack.push_back(0);
}

sa::Analyzer::~Analyzer()
{
  trace("Destroy analyzer " << unit->process_name());
  // Make sure all occurrence lists and file and symbol references are cleared
  // while the project is locked.  Removing an occurrence may remove the
  // occurring entity, which may require the project to be locked, because
  // entities such as symbols erase themselves from the project's symbol table.
  assert(occurrences.empty());
  assert(scope_data.empty());
  assert(diagnostics.empty());
  assert(sections.empty());
  assert(non_utf8_files.empty());
  assert(parent_stack.size() == 1);
  assert(scope_stack.empty());
}

base::ptr<sa::Section> sa::Analyzer::create_section(
  const std::string &name,
  const std::string &member_name
)
{
  base::ptr<Section> section = base::ptr<Section>::create(
    unit, name, member_name
  );
  sections.emplace_back(section);
  trace("created section " << *section);
  return section;
}

sa::Section *sa::Analyzer::get_section() const
{
  assert_(sections.size() == 1, sections.size());
  return sections.front();
}

void sa::Analyzer::add_memory_section(
  const std::string &name,
  const std::string &runtime_region,
  const std::string &load_region
)
{
  // TODO
  (void)name;
  (void)runtime_region;
  (void)load_region;
}

void sa::Analyzer::add_memory_region(
  const std::string &name,
  size_t origin,
  size_t size
)
{
  // TODO
  (void)name;
  (void)origin;
  (void)size;
}

base::ptr<sa::File> sa::Analyzer::get_file(const std::string &path)
{
  trace_nest("Analyzer get_file " << path);
  Project::Lock lock(project);
  std::string abs_path =
    base::get_absolute_path(path, project->get_build_path());
  return project->get_file(abs_path);
}

base::ptr<sa::Symbol> sa::Analyzer::get_global_symbol(
  const std::string &link_name
)
{
  trace("get global symbol " << link_name);
  return project->get_global_symbol(link_name);
}

base::ptr<sa::Symbol> sa::Analyzer::get_local_symbol(
  EntityKind kind,
  const std::string &user_name,
  const FileLocation &ref_location
)
{
  trace("get local symbol " << kind << " " << user_name << " " << ref_location);
  assert_(!is_global_symbol_kind(kind), kind);
  return project->get_local_symbol(
    kind, user_name, ref_location, current_scope()
  );
}

sa::Occurrence *sa::Analyzer::current_scope() const
{
  assert(!parent_stack.empty());
  return parent_stack.back();
}

sa::Occurrence *sa::Analyzer::top_scope() const
{
  return parent_stack.size() < 2 ? 0 : parent_stack.at(1);
}

void sa::Analyzer::add_include(
  base::ptr<File> const &included_file,
  base::ptr<File> const &including_file,
  Range range,
  const std::string &hdir_path
)
{
  trace_nest("add include of " << *included_file << " from " << *including_file
    << " " << range << " hdir " << hdir_path
  );
  base::ptr<Hdir> hdir;
  if (!hdir_path.empty()) {
    project->lock();
    hdir = project->get_hdir(
      base::get_absolute_path(hdir_path, project->get_build_path())
    );
    project->unlock();
  }
  auto occurrence = including_file->get_occurrence(
    OccurrenceKind_include, OccurrenceStyle_unspecified,
    included_file, range, hdir
  );
  occurrences.push_back(occurrence);
  trace("add include " << *occurrence);
}

sa::Occurrence *sa::Analyzer::add_occurrence(
  base::ptr<Symbol> const &symbol,
  EntityKind ekind,
  OccurrenceKind okind,
  Section *section,
  base::ptr<File> const &file,
  Range range,
  bool add_to_section
)
{
  OccurrenceStyle style = OccurrenceStyle_unspecified;
  switch (ekind) {
    case EntityKind_global_variable:
      style = OccurrenceStyle_data;
      break;
    case EntityKind_global_function:
      style = OccurrenceStyle_function;
      break;
    case EntityKind_virtual_function:
      style = OccurrenceStyle_virtual_function;
      break;
    default:
      assert_(ekind == symbol->kind, ekind << " <> " << symbol->kind
        << ": " << *symbol << " " << *file << " " << range
      );
  }
  trace("style: " << style);
  return add_occurrence(symbol, okind, style, section, file, range,
    add_to_section
  );
}

sa::Occurrence *sa::Analyzer::add_occurrence(
  base::ptr<Symbol> const &symbol,
  OccurrenceKind kind,
  OccurrenceStyle style,
  Section *section,
  base::ptr<File> const &file,
  Range range,
  bool add_to_section
)
{
  base::ptr<Occurrence> occurrence = file->get_occurrence(
    kind, style, symbol, range
  );
  trace_nest("add occurrence " << *occurrence
    << " add-to-section=" << add_to_section
  );
  assert(base::is_valid(section));
  occurrences.push_back(occurrence);
  current_occurrence = occurrence;
  //
  // Save linker-relevant occurrences separately.
  if (add_to_section && symbol->is_global()) {
    add_global_occurrence_to_section(occurrence, section);
  }
  if (!scope_stack.empty()) {
    // This is a nested occurrence; maintain scope information.
    Scope *scope = scope_stack.back();
    if (!scope->count) {
      scope->index = occurrences.size();
    }
    scope->count++;
  }
  return occurrence;
}

void sa::Analyzer::add_global_occurrence_to_section(
  sa::Occurrence *occurrence,
  Section *section
)
{
  trace("in section " << *section << " add " << *occurrence);
  assert(occurrence->entity->is_global_symbol());
  section->add_occurrence(occurrence);
  // The linker no longer implicly assumes that a use of a global symbol
  // implies that a definition is required.
  if (occurrence->kind == OccurrenceKind_use) {
    if (occurrence->style != OccurrenceStyle_virtual_function) {
      trace("Require definition");
      section->require_definition(occurrence->entity->as_global_symbol());
    }
  }
  if (occurrence->kind == OccurrenceKind_weak_use) {
    trace("Weakly require definition");
    section->weakly_require_definition(occurrence->entity->as_global_symbol());
  }
}

// For GDB workaround
void sa::Analyzer::add_empty_loop(
  base::ptr<File> const &file,
  Range range
)
{
  assert(!range.is_empty());
  base::ptr<EmptyLoop> empty_loop = file->get_empty_loop(range);
  trace_nest("add empty loop " << *empty_loop);
  empty_loops.push_back(empty_loop);
}

void sa::Analyzer::require_definition(
  base::ptr<Symbol> const &symbol,
  Section *section
)
{
  trace("require a definition of " << *symbol << " in " << *section);
  assert_(symbol->is_global(), *symbol);
  section->require_definition(symbol->as_global_symbol());
}

void sa::Analyzer::weakly_require_definition(
  base::ptr<Symbol> const &symbol,
  Section *section
)
{
  trace("require a definition of " << *symbol << " in " << *section);
  assert_(symbol->is_global(), *symbol);
  section->weakly_require_definition(symbol->as_global_symbol());
}

void sa::Analyzer::provide_weak_definition(
  base::ptr<Symbol> const &symbol,
  Section *section
)
{
  trace("provide a weak definition of " << *symbol << " in " << *section);
  assert_(symbol->is_global(), *symbol);
  //section->provide_weak_definition(symbol->as_global_symbol());
}

void sa::Analyzer::open_scope()
{
  assert(current_occurrence);
  assert_(current_occurrence->can_be_scope(), *current_occurrence);
  parent_stack.push_back(current_occurrence);
  Scope *scope = &scope_data[current_occurrence];
  scope_stack.push_back(scope);
  // Make sure there cannot be two scopes for the same occurrence.
  current_occurrence = 0;
}

void sa::Analyzer::close_scope()
{
  assert(!scope_stack.empty());
  Scope *scope = scope_stack.back();
  scope_stack.pop_back();
  if (!scope_stack.empty()) {
    scope_stack.back()->count += scope->count;
  }
  assert(!parent_stack.empty());
  Occurrence *parent = parent_stack.back();
  assert(base::is_valid(parent));
  parent_stack.pop_back();
}

void sa::Analyzer::report_diagnostic(
  const std::string &message,
  Severity severity,
  base::ptr<File> const &file,
  Location location,
  bool is_linker_relevant
)
{
  project->lock();
  trace_nest("Diagnostic: " << message);
  trace("Report " << severity << ": " << message << " at "
    << (file ? file->get_name() : "<no-file>")
    << "." << location << " in " << unit->process_name()
    << " linker-relevant=" << is_linker_relevant
  );
  auto diagnostic = file ? file->get_diagnostic(message, severity, location)
    : project->get_diagnostic(message, severity);
  project->unlock();
  push_diagnostic(diagnostic, is_linker_relevant);
}

void sa::Analyzer::report_diagnostic(
  const std::string &message,
  Severity severity,
  FileLocation location,
  bool is_linker_relevant
)
{
  report_diagnostic(message, severity, location.file, location, is_linker_relevant);
}

void sa::Analyzer::report_missing_header(
  const std::string &name
)
{
  missing_headers.push_back(name);
}

void sa::Analyzer::report_non_utf8_file(base::ptr<File> const &file)
{
  non_utf8_files.insert(file);
}

inline bool ends_with(std::string const &value, std::string const &ending)
{
  if (ending.size() > value.size()) return false;
  return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

void sa::Analyzer::push_diagnostic(
  Diagnostic *diagnostic,
  bool is_linker_relevant
)
{
  trace("push diagnostic " << *diagnostic << " is-linker-relevant=" <<
    is_linker_relevant
  );
  diagnostics.push_back(diagnostic);
  if (is_linker_relevant) {
    has_linker_relevant_error = true;
  }
}

void sa::Analyzer::execute(std::string const &flag_buffer)
{
  (void)toolchain;
  (void)compiler_flags;
  (void)analysis_flags;
  trace_nest("execute analysis of " << unit->process_name());
  InternalErrorAction internal_error_handler(project);
  debug_string_context("analyzing", unit->process_name());
  base::Timer::Scope scope(overall_timer);
  assert(is_valid(this));
  auto process_name = unit->process_name();
  bool cache_loaded;
  {
    base::Timer::Scope scope(prelude_timer);
    Project::Lock lock(project);
    if (unit->cancelled()) {
      trace("Cancelled analysis for " << process_name);
      return;
    }
    trace("start analysis for " << process_name);
    cache_loaded = unit->cache_loaded;
    unit->cache_loaded = true;
    trace("orig cache loaded: " << cache_loaded);
  }
  if (!cache_loaded && read_cache(flag_buffer)) {
    trace("loaded cache from " << get_cache_path());
    return;
  }
  analysis_timer.start();
  trace("analyze          " << process_name);
  //trace_code(print_flags(flag_buffer));
  //trace_code(print_includes(flag_buffer));

  // Did the analysis succeed?
  success = run(flag_buffer);
  analysis_timer.stop();
  Project::Lock lock(project);
  trace("analysis done, success=" << success << ", project locked to report");
  // If the analysis was cancelled, abort. It is essential to do this before
  // accessing the compilation unit or the main files, as these may have been
  // deleted. It is also essential to clear the results first, because an
  // analysis with non-empty results cannot be deleted.
  if (unit->cancelled()) {
    trace("cancelled during analysis of " << process_name);
    return;
  }
  if (success && !has_alternative_content) {
    trace_nest("Write cache for " << unit->process_name());
    write_cache(flag_buffer);
  }
}

std::string sa::Analyzer::get_cache_path() const
{
  return cache_path;
}

static const char *magic_string = "embeetle cache v93";

void sa::Analyzer::write_cache(std::string const &flag_buffer)
{
  base::Timer::Scope scope(cache_write_timer);
  debug_string_context("writing cache", unit->process_name());
  if (sections.size()!=1) {
    trace("write cache for " << sections.size() << " sections not implemented");
    base::remove(get_cache_path());
    return;
  }
  if (!non_utf8_files.empty()) {
    trace("write cache for non-UTF8 files not implemented");
    base::remove(get_cache_path());
    return;
  }
  trace_nest("write cache for " << unit->process_name()
    << " to " << get_cache_path()
    << " (" << occurrences.size() << " occurrences)"
  );
  const std::string &project_path = project->get_project_path();
  const std::string &toolchain_path = unit->get_toolchain();
  // Collect all files, hdirs and symbols used in this compilation unit, for
  // counting and iteration. Collect them in a map so that a unique id can be
  // assigned to each file, hdir and symbol later.
  std::map<File*, unsigned> files;
  files[unit->file];
  std::map<Hdir*, unsigned> hdirs;
  for (auto occurrence: occurrences) {
    files[occurrence->file];
    if (occurrence->kind == OccurrenceKind_include) {
      trace("cache occurrence #include " << occurrence->file->get_name());
      files[occurrence->entity->get_file()];
      hdirs[occurrence.static_cast_to<Inclusion>()->hdir];
    } else if (occurrence->entity->is_local_symbol()) {
      files[occurrence->entity->as_local_symbol()->get_ref_location().file];
    }
  }
  for (auto empty_loop: empty_loops) {
    files[empty_loop->file];
  }
  if (unit->cancelled()) {
    trace("cancelled " << unit->process_name());
    return;
  }
  std::ofstream out;
  base::open_for_writing(out, get_cache_path(),
    base::get_parent_path(project->get_cache_path()).size()    
  );
  if (!out.is_open()) {
    trace("cannot open output " << get_cache_path());
    return;
  }
  out << magic_string << "\n";
  out << has_linker_relevant_error << "\n";

  // Flags
  const char *begin = flag_buffer.data();
  const char *end = begin + flag_buffer.size();
  out << flag_buffer.size() << "\n";
  for (const char *flag = begin; flag < end; flag += strlen(flag) + 1) {
    out << flag << "\n";
  }

  // Files
  out << files.size() << "\n";
  trace(files.size() << " files");
  unsigned file_count = 0;
  for (auto &[file, index]: files) {
    index = file_count++;
    std::string path = file->get_path();
    char code = '.';
    path = base::get_natural_path(path, project_path);
    if (base::is_absolute_path(path)) {
      // Replace the toolchain folder by _. Moving the toolchain folder to a
      // different location will not invalidate the cache, as long as the path
      // of toolchain headers relative to the toolchain folder remains the same.
      path = base::get_natural_path(path, toolchain_path);
      code = base::is_absolute_path(path) ? '/' : '_';
    }
    trace("file " << index << " " << code << " " << path);
    out << base::get_signature(file->get_path()) << code << path << "\n";
  }
  assert(file_count == files.size());

  // Hdirs
  out << hdirs.size() << "\n";
  unsigned hdir_count = 0;
  for (auto &[hdir, index]: hdirs) {
    index = hdir_count++;
    std::string path;
    char code = '-';
    if (hdir) {
      path = hdir->path;
      code = '.';
      path = base::get_natural_path(path, project_path);
      if (base::is_absolute_path(path)) {
        path = base::get_natural_path(path, toolchain_path);
        code = base::is_absolute_path(path) ? '/' : '_';
      }
    }
    out << code << path << "\n";
  }
  assert(hdir_count == hdirs.size());
  
  // Diagnostics
  out << diagnostics.size() << "\n";
  const char severity_codes[] = "we";
  for (auto diagnostic: diagnostics) {
    assert(diagnostic->get_severity() < (sizeof(severity_codes)-1));
    out << files[diagnostic->file]
        << "@" << diagnostic->location
        << severity_codes[diagnostic->get_severity()]
        << diagnostic->get_message()
        << "\n";
  }

  // Missing headers
  out << missing_headers.size() << "\n";
  for (auto missing_header: missing_headers) {
    out << missing_header << "\n";
  }

  // Symbols and occurrences
  trace(occurrences.size() << " occurrences");
  out << occurrences.size() << "\n";
  std::map<Symbol*, unsigned> symbols;
  std::map<Occurrence*, unsigned> scopes;
  //std::set<Occurrence*> occurrences; (void)occurrences;
  unsigned next_scope_index = 0;
  for (auto occurrence: occurrences) {
    //assert_(occurrences.find(occurrence) != occurrences.end(), *occurrence);
    //occurrences.insert(occurrence);
    write_cache_occurrence(occurrence, symbols, scopes, files, hdirs,
      next_scope_index, out
    );
  }

  // Empty loops
  trace(empty_loops.size() << " empty loops");
  out << empty_loops.size() << "\n";
  for (auto empty_loop: empty_loops) {
    auto file_it = files.find(empty_loop->file);
    assert(file_it != files.end());
    out << file_it->second
        << "@" << empty_loop->get_range().begin
        << "-" << empty_loop->get_range().end
        << "\n"
      ;
  }

  // Required globals
  const auto &required_globals = sections.front()->get_required_globals();
  trace(required_globals.size() << " required globals");
  out << required_globals.size() << "\n";
  for (auto symbol: required_globals) {
    unsigned index = write_cache_symbol(symbol, symbols, scopes, files, out);
    trace("index for " << *symbol << " is " << index);
    out << index << "\n";
  }

  // Weakly required globals
  const auto &weakly_required_globals =
    sections.front()->get_weakly_required_globals();
  trace(weakly_required_globals.size() << " weakly_required globals");
  out << weakly_required_globals.size() << "\n";
  for (auto symbol: weakly_required_globals) {
    unsigned index = write_cache_symbol(symbol, symbols, scopes, files, out);
    trace("index for " << *symbol << " is " << index);
    out << index << "\n";
  }

  out.close();
  trace("cache written for " << unit->process_name());
  assert(base::is_readable(get_cache_path()));
}

void sa::Analyzer::write_cache_occurrence(
  Occurrence *occurrence,
  std::map<Symbol*, unsigned> &symbols,
  std::map<Occurrence*, unsigned> &scopes,
  std::map<File*, unsigned> const &files,
  std::map<Hdir*, unsigned> const &hdirs,
  unsigned &next_scope_index,
  std::ofstream &out
)
{
  assert(occurrence);
  trace_nest("write " << *occurrence);
  unsigned symbol_index;
  if (occurrence->kind != OccurrenceKind_include) {
    symbol_index = write_cache_symbol(
      occurrence->entity->get_symbol(), symbols, scopes, files, out
    );
  }
  const char kind_codes[] = "DCWdwuvi";
  const char style_codes[] = "udfv";
  assert(occurrence->kind < (sizeof(kind_codes)-1));
  assert(occurrence->style < (sizeof(style_codes)-1));
  auto file_it = files.find(occurrence->file);
  assert(file_it != files.end());
  out << file_it->second
      << "@" << occurrence->get_range().begin
      << "-" << occurrence->get_range().end
      << kind_codes[occurrence->kind]
      << style_codes[occurrence->style]
    ;
  if (occurrence->kind == OccurrenceKind_include) {
    auto file_it = files.find(occurrence->entity->get_file());
    assert(file_it != files.end());
    auto hdir_it = hdirs.find(static_cast<Inclusion*>(occurrence)->hdir);
    assert(hdir_it != hdirs.end());
    out << file_it->second << " " << hdir_it->second;
  } else {
    out << symbol_index;
  }
  out << "\n";
  if (occurrence->can_be_scope()) {
    trace("scope " << next_scope_index << ": " << *occurrence);
    // Scopes are always written before their nested symbols.  Occurrences -
    // including scopes - may be instantiated more than once in the same
    // compilation unit; therefore, to determine the next scope index, we cannot
    // rely on the size of the scopes set. We need to uncondionally increment
    // the scope index, to match the behavior while reading the cached data.
    unsigned scope_index = next_scope_index++;
    scopes[occurrence] = scope_index;
    trace(" `--> save in scope set with index " << scope_index);
  }
}

// Get the index for a cached symbol;  write the symbol to cache first if needed
unsigned sa::Analyzer::write_cache_symbol(
  Symbol *symbol,
  std::map<Symbol*, unsigned> &symbols,
  std::map<Occurrence*, unsigned> &scopes,
  std::map<File*, unsigned> const &files,
  std::ofstream &out
)
{
  assert(base::is_valid(symbol));
  auto it = symbols.find(symbol);
  if (it != symbols.end()) {
    return it->second;
  }
  EntityKind kind = symbol->kind;
  assert_(kind < (sizeof(EntityKind_codes)-1), (int)kind);
  trace("write " << *symbol << " (kind-code=" << EntityKind_codes[kind] << ")");
  assert_(EntityKind_codes[kind] != ' ', kind);
  out << EntityKind_codes[kind];
  if (is_global_symbol_kind(kind)) {
    out << symbol->as_global_symbol()->link_name << "\n";
  } else {
    out << symbol->name << "\n";
    LocalSymbol *local_symbol = symbol->as_local_symbol();
    assert(base::is_valid_or_null(local_symbol->ref_scope));
    assert(base::is_valid(local_symbol->get_ref_location().file));
    auto file_it = files.find(local_symbol->get_ref_location().file);
    assert(file_it != files.end());
    out << file_it->second << "@" << local_symbol->get_ref_location().offset;
    Occurrence *scope = local_symbol->ref_scope;
    if (scope) {
      trace(" `--> in " << *scope);
      auto it = scopes.find(scope);
      // Scopes are always written before their nested symbols
      assert_(it != scopes.end(), "not saved: " << *scope);
      out << "^" << it->second;
      trace(" `--> scope index: " << it->second);
    }
    out << "\n";
  }
  unsigned symbol_id = symbols.size();
  symbols[symbol] = symbol_id;
  return symbol_id;
}

bool sa::Analyzer::read_cache(const std::string &expected_flags)
{
  trace_nest("read cache");
  debug_string_context("reading cache", unit->process_name());
  bool status = false;
  std::vector<base::ptr<File>> files;
  std::vector<base::ptr<sa::Hdir>> hdirs;
  std::vector<base::ptr<Symbol>> symbols;
  create_section("");
  {
    base::Timer::Scope scope(cache_read_timer);
    //debug_context("read cache");
    if (sections.size() != 1) {
      trace("cache reading only implemented for one section");
      return false;
    }
    trace_nest("read cache for " << unit->process_name()
      << " from " << get_cache_path());
    status = read_cache_core(expected_flags, files, hdirs, symbols);
    trace("cache loaded: " << status);
  }
  project->lock();
  {
    trace_nest("project locked");
    // Clear the files, symbols and hdirs lists while the project is locked,
    // because removing references to reference counted files, symbols or hdirs
    // may delete some files, symbols or hdirs which requires the project to be
    // locked. It is not good to rely on automatic destruction when the lists
    // goes out of scope, because at that point the project is no longer locked.
    symbols.clear();
    hdirs.clear();
    files.clear();
    if (status) {
      success = true;
      from_cache = true;
    } else {
      // Cache reading failed, so normal analysis will proceed. Normal analysis
      // will create a section, so sections created here must be cleared.
      sections.clear();
    }
  }
  project->unlock();
  return status;
}

bool sa::Analyzer::read_cache_core(
  const std::string &expected_flags,
  std::vector<base::ptr<File>> &files,
  std::vector<base::ptr<sa::Hdir>> &hdirs,
  std::vector<base::ptr<Symbol>> &symbols
)
{
  //debug_context("read cache core");
  trace("read cache core");
  assert(sections.size() == 1);
  Section *section = sections.front();
  const std::string &project_path = project->get_project_path();
  std::string toolchain_path;
  {
    Project::Lock lock(project);
    toolchain_path = unit->get_toolchain();
  }
  std::ifstream in_stream;
  base::open_for_reading(in_stream, get_cache_path());
  if (!in_stream.is_open()) {
    trace("cannot read cache " << get_cache_path());
    return false;
  }

#if 0
  // two steps
  std::stringstream in;
  {
    base::Timer::Scope scope(cache_file_read_timer);
    in << in_stream.rdbuf();
  }
#else
  // direct
  std::ifstream &in = in_stream;
#endif
    
  std::string line;
  std::getline(in, line);
  if (line != magic_string) {
    trace("bad magic string " << line);
    return false;
  }
  in >> has_linker_relevant_error;
  if (!in) {
    trace("no linker-relevant-error flag");
    return false;
  }
  
  // Flags
  {
    //debug_context("check flags");
    size_t flag_size;
    in >> flag_size;
    if (!in) {
      trace("no flag size");
      return false;
    }
    trace("flag size: " << flag_size);
    in.get();
    // Flag buffer size is flag_size +1 for final \0!
    std::unique_ptr<char[]> flag_buffer(new char [flag_size+1]);
    in.read(flag_buffer.get(), flag_size);
    if (!in) {
      trace("not enough flag data");
      return false;
    }
    for (size_t i = flag_size; i--; ) {
      if (flag_buffer[i] == '\n') {
        flag_buffer[i] = 0;
      }
    }
    flag_buffer[flag_size] = 0;
    if (memcmp(flag_buffer.get(), expected_flags.data(), flag_size+1)) {
      trace("flags changed");
#if 0
      trace_code(
        debug_stream << "Expected:";
        print_flags(expected_flags);
        debug_stream << "Found:";
        print_flags(std::string(flag_buffer.get(), flag_size));
      );
#endif
      return false;
    }
  }

  // Files
  trace("reading files ...");
  unsigned file_count;
  in >> file_count;
  if (!in) {
    trace("no file count");
    return false;
  }
  trace("file count: " << file_count);
  files.reserve(file_count);
  while (file_count--) {
    uint64_t signature;
    char code;
    std::string path;
    in >> signature >> code;
    std::getline(in, path);
    if (!in) {
      trace("no signature-code-path");
      return false;
    }
    //trace("file: " << signature << " " << code << " " << path);
    switch (code) {
      case '.':
        path = project_path + "/" + path;
        break;
      case '_':
        path = toolchain_path + "/" + path;
        break;
      case '/':
        break;
      default:
        trace("bad code " << code);
        return false;
    }
    if (base::get_signature(path) != signature) {
      trace("signature changed from " << signature << " to "
        << base::get_signature(path) << " for " << path
      );
      return false;
    }
    base::ptr<File> file = get_file(path);
    files.push_back(file);
  }

  // Hdirs
  trace("reading hdirs ...");
  unsigned hdir_count;
  in >> hdir_count;
  if (!in) {
    trace("no hdir count");
    return false;
  }
  trace("hdir count: " << hdir_count);
  hdirs.reserve(hdir_count);
  while (hdir_count--) {
    char code;
    std::string path;
    in >> code;
    std::getline(in, path);
    if (!in) {
      trace("no hdir code-path");
      return false;
    }
    //trace("hdir: " << code << " " << path);
    switch (code) {
      case '-':
        if (!path.empty()) {
          trace("non-empty path for null hdir " << path);
          return false;
        }
        break;
      case '.':
        path = project_path + "/" + path;
        break;
      case '_':
        path = toolchain_path + "/" + path;
        break;
      case '/':
        break;
      default:
        trace("bad code " << code);
        return false;
    }
    base::ptr<Hdir> hdir = 0;
    if (!path.empty()) {
      project->lock();
      hdir = project->get_hdir(path);
      project->unlock();
    }
    hdirs.push_back(hdir);
  }

  // Diagnostics
  trace("reading diagnostics ...");
  unsigned diagnostic_count;
  in >> diagnostic_count;
  if (!in) {
    trace("no diagnostic count");
    return false;
  }
  trace("found " << diagnostic_count << " cached diagnostics");
  assert(diagnostics.empty());
  diagnostics.reserve(diagnostic_count);
  while (diagnostic_count--) {
    unsigned file_index;
    char sep1;
    unsigned offset;
    char severity_code;
    std::string message;
    in >> file_index >> sep1 >> offset >> severity_code;
    std::getline(in, message);
    if (!in) {
      trace("no diagnostic");
      return false;
    }
    if (file_index >= files.size()) {
      trace("bad diagnostic file index " << file_index);
      return false;
    }
    auto file = files[file_index];
    assert(base::is_valid(file));
    if (sep1 != '@') {
      trace("bad diagnostic separator " << sep1);
      return false;
    }
    Severity severity;
    switch (severity_code) {
      case 'w': severity = Severity_warning; break;
      case 'e': severity = Severity_error; break;
      default:
        trace("bad severity code " << severity_code);
        return false;
    }
    Location location(offset);
    project->lock();
    auto diagnostic = file->get_diagnostic(message, severity, location);
    //trace("got diagnostic: " << *diagnostic);
    project->unlock();
    // TODO:  `is_linker_relevant` is not cached! Best to do this globally
    // for the unit
    push_diagnostic(diagnostic, false);
  }

  // Missing headers
  trace("reading missing headers ...");
  unsigned missing_header_count;
  in >> missing_header_count;
  if (!in) {
    trace("no missing header count");
    return false;
  }
  in.ignore(100,'\n'); // why?
  trace("missing header count: " << missing_header_count);
  assert(missing_headers.empty());
  missing_headers.reserve(missing_header_count);
  while (missing_header_count--) {
    std::string path;
    std::getline(in, path);
    if (!in || path.empty()) {
      trace("no missing header " << path << " " << in.good() << "#"
        << in.eof() << "#" << in.fail() << "#" << in.bad() << "#" << !!in
      );
      return false;
    }
    trace("missing header " << path);
    assert_(!path.empty(), unit->process_name());
    missing_headers.push_back(path);
  }
  
  // Symbols and occurrences
  trace("reading symbol and occurrences ...");
  unsigned occurrence_count;
  in >> occurrence_count;
  if (!in) {
    trace("no occurrence count");
    return false;
  }
  trace("occurrence count: " << occurrence_count);
  assert(occurrences.empty());
  occurrences.reserve(occurrence_count);
  // Skip end-of-line of previous line: essential, because peek() must return
  // the first character of the *next* line to be able to determine whether it
  // should read an occurrence or a symbol.
  in.ignore(); 
  std::vector<Occurrence*> scopes;
  while (occurrence_count) {
    // Is this an occurrence or a symbol?
    if (isdigit(in.peek())) {
      // It is an occurrence
      //trace("read occurrence "  << occurrence_count);
      occurrence_count--;
      unsigned file_index;
      char sep1;
      unsigned begin_offset;
      char sep2;
      unsigned end_offset;
      char kind_code;
      char style_code;
      in >> file_index >> sep1 >> begin_offset >> sep2 >> end_offset
         >> kind_code >> style_code;
      if (!in) {
        trace("no occurrence");
        return false;
      }
      if (file_index >= files.size()) {
        trace("bad occurrence file index " << file_index);
        return false;
      }
      auto file = files[file_index];
      assert(base::is_valid(file));
      if (sep1 != '@') {
        trace("bad occurrence separator " << sep1);
        return false;
      }
      if (sep2 != '-') {
        trace("bad occurrence separator " << sep2);
        return false;
      }
      OccurrenceKind kind;
      switch (kind_code) {
        case 'D': kind = OccurrenceKind_definition; break;
        case 'C': kind = OccurrenceKind_tentative_definition; break;
        case 'W': kind = OccurrenceKind_weak_definition; break;
        case 'd': kind = OccurrenceKind_declaration; break;
        case 'w': kind = OccurrenceKind_weak_declaration; break;
        case 'u': kind = OccurrenceKind_use; break;
        case 'v': kind = OccurrenceKind_weak_use; break;
        case 'i': kind = OccurrenceKind_include; break;
        default:
          trace("bad occurrence kind code " << kind_code);
          return false;
      }
      OccurrenceStyle style;
      switch (style_code) {
        case 'u': style = OccurrenceStyle_unspecified; break;
        case 'd': style = OccurrenceStyle_data; break;
        case 'f': style = OccurrenceStyle_function; break;
        case 'v': style = OccurrenceStyle_virtual_function; break;
        default:
          trace("bad occurrence style code " << style_code);
          return false;
      }
      base::ptr<Entity> entity;
      base::ptr<Hdir> hdir;
      bool is_global = false;
      if (kind == OccurrenceKind_include) {
        unsigned file_index;
        unsigned hdir_index;
        in >> file_index >> hdir_index;
        if (!in) {
          trace("no file entity-hdir");
          return false;
        }
        if (file_index >= files.size()) {
          trace("bad entity file index " << file_index);
          return false;
        }
        entity = files[file_index];
        assert(base::is_valid(entity));
        if (hdir_index >= hdirs.size()) {
          trace("bad entity hdir index " << hdir_index);
          return false;
        }
        hdir = hdirs[hdir_index];
        assert(base::is_valid_or_null(hdir));
      } else {
        unsigned symbol_index;
        in >> symbol_index;
        if (!in) {
          trace("no symbol");
          return false;
        }
        if (symbol_index >= symbols.size()) {
          trace("bad symbol index " << symbol_index);
          return false;
        }
        base::ptr<Symbol> symbol = symbols[symbol_index];
        EntityKind symbol_kind = symbol->kind;
        is_global = is_global_symbol_kind(symbol_kind);
        entity = symbol;
      }
      assert(base::is_valid(entity));
      Range range(begin_offset, end_offset);
      auto occurrence = file->get_occurrence(kind, style, entity, range, hdir);
      //trace("got occurrence " << *occurrence
      //  << " can-be-scope=" << occurrence->can_be_scope()
      //);
      occurrences.push_back(occurrence);
      if (is_global) {
        section->add_occurrence(occurrence);
      }
      if (occurrence->can_be_scope()) {
        //trace("read scope " << scopes.size() << " " << *occurrence);
        scopes.push_back(occurrence);
      }
      // Skip end-of-line of previous line: essential, because peek() must
      // return the first character of the *next* line to be able to determine
      // whether it should read an occurrence or a symbol.
      in.ignore();
    } else {
      // It is a symbol
      if (!read_cache_symbol(in, symbols, files, scopes)) {
        return false;
      }
    }
  }

  // Empty loops
  trace("reading empty loops ...");
  unsigned empty_loop_count;
  in >> empty_loop_count;
  if (!in) {
    trace("no empty_loop count");
    return false;
  }
  trace("empty loop count: " << empty_loop_count);
  assert(empty_loops.empty());
  empty_loops.reserve(empty_loop_count);
  while (empty_loop_count) {
    empty_loop_count--;
    // Skip end-of-line of previous line.
    in.ignore(); 
    unsigned file_index;
    char sep1;
    unsigned begin_offset;
    char sep2;
    unsigned end_offset;
    in >> file_index >> sep1 >> begin_offset >> sep2 >> end_offset;
    if (!in) {
      trace("no empty loop");
      return false;
    }
    if (file_index >= files.size()) {
      trace("bad empty loop file index " << file_index);
      return false;
    }
    auto file = files[file_index];
    assert(base::is_valid(file));
    if (sep1 != '@') {
      trace("bad empty loop separator " << sep1);
      return false;
    }
    if (sep2 != '-') {
      trace("bad empty loop separator " << sep2);
      return false;
    }
    Range range(begin_offset, end_offset);
    auto empty_loop = file->get_empty_loop(range);
    //trace("got empty loop " << *empty_loop);
    empty_loops.push_back(empty_loop);
  }

  // Required globals
  trace("reading required globals ...");
  unsigned required_global_count;
  in >> required_global_count;
  if (!in) {
    trace("no required_global count");
    return false;
  }
  // Skip end-of-line 
  in.ignore();
  trace("required global count: " << required_global_count);
  while (required_global_count) {
    if (isdigit(in.peek())) {
      // It is a required global index
      required_global_count--;
      unsigned symbol_index;
      in >> symbol_index;
      if (!in) {
        trace("no required symbol index");
        return false;
      }
      // Skip end-of-line 
      in.ignore();
      if (symbol_index >= symbols.size()) {
        trace("bad required symbol index " << symbol_index);
        return false;
      }
      base::ptr<Symbol> symbol = symbols[symbol_index];
      if (!symbol->is_global_symbol()) {
        trace("required symbol is not global " << symbol->name);
        return false;
      }
      trace("require definition for " << *symbol);
      require_definition(symbol, section);
    } else {
      // It is a new symbol
      if (!read_cache_symbol(in, symbols, files, scopes)) {
        trace("failed");
        return false;
      }
    }
  }

  // Weakly required globals
  trace("reading weakly required globals ...");
  unsigned weakly_required_global_count;
  in >> weakly_required_global_count;
  if (!in) {
    trace("no weakly_required_global count");
    return false;
  }
  // Skip end-of-line 
  in.ignore();
  trace("weakly required global count: " << weakly_required_global_count);
  while (weakly_required_global_count) {
    if (isdigit(in.peek())) {
      // It is a required global index
      weakly_required_global_count--;
      unsigned symbol_index;
      in >> symbol_index;
      if (!in) {
        trace("no required symbol index");
        return false;
      }
      // Skip end-of-line 
      in.ignore();
      if (symbol_index >= symbols.size()) {
        trace("bad required symbol index " << symbol_index);
        return false;
      }
      base::ptr<Symbol> symbol = symbols[symbol_index];
      if (!symbol->is_global_symbol()) {
        trace("weakly required symbol is not global " << symbol->name);
        return false;
      }
      trace("weakly require definition for " << *symbol);
      weakly_require_definition(symbol, section);
    } else {
      // It is a new symbol
      if (!read_cache_symbol(in, symbols, files, scopes)) {
        trace("failed");
        return false;
      }
    }
  }
  return true;
}

bool sa::Analyzer::read_cache_symbol(
  std::istream &in,
  std::vector<base::ptr<Symbol>> &symbols,
  std::vector<base::ptr<File>> &files,
  std::vector<Occurrence*> &scopes
)
{
  //trace_nest("read symbol");
  char kind_code;
  in >> kind_code;
  if (!in) {
    trace("unexpected end of file while reading symbol");
    return false;
  }
  //trace("  `-> kind code " << (unsigned)kind_code << " " << kind_code);
  EntityKind kind;
  switch (kind_code) {
    case 'C': kind = EntityKind_global_symbol; break;
    case 'V': kind = EntityKind_global_variable; break;
    case 'F': kind = EntityKind_global_function; break;
    case 'b': kind = EntityKind_virtual_function; break;
    case 'w': kind = EntityKind_global_variable_template; break;
    case 'g': kind = EntityKind_global_function_template; break;
    case 'd': kind = EntityKind_class_template; break;
    case 'h': kind = EntityKind_template_parameter; break;
    case 'r': kind = EntityKind_struct; break;
    case 'u': kind = EntityKind_union; break;
    case 'n': kind = EntityKind_enum; break;
    case 'N': kind = EntityKind_section; break;
    case 'c': kind = EntityKind_local_symbol; break;
    case 'v': kind = EntityKind_local_static_variable; break;
    case 's': kind = EntityKind_static_variable; break;
    case 'a': kind = EntityKind_automatic_variable; break;
    case 'p': kind = EntityKind_parameter; break;
    case 'f': kind = EntityKind_local_function; break;
    case 't': kind = EntityKind_type; break;
    case 'm': kind = EntityKind_field; break;
    case 'e': kind = EntityKind_enum_constant; break;
    case 'M': kind = EntityKind_macro; break;
    case 'l': kind = EntityKind_class; break;
    case 'o': kind = EntityKind_other; break;
    default:
      trace("bad symbol kind code " << kind_code);
      return false;
  }
  std::string name;
  std::getline(in, name);
  FileLocation location;
  Occurrence *scope = 0;
  if (is_global_symbol_kind(kind)) {
    //trace("create " << kind << " " << name);
    if (kind != EntityKind_global_symbol) {
      trace("unexpected kind " << kind);
      return false;
    }
    symbols.push_back(get_global_symbol(name));
  } else {
    unsigned file_index;
    char sep1;
    in >> file_index >> sep1 >> location.offset;
    if (!in) {
      trace("no local symbol location for " << kind << " " << name);
      return false;
    }
    if (file_index >= files.size()) {
      trace("bad local symbol file index " << file_index);
      return false;
    }
    location.file = files[file_index];
    assert(base::is_valid(location.file));
    if (sep1 != '@') {
      trace("bad local symbol separator #1 " << sep1);
      return false;
    }
    if (in.peek() == '^') {
      char sep4;
      unsigned scope_index;
      in >> sep4 >> scope_index;
      if (!in) {
        trace("missing scope index for " << kind << " " << name);
        return false;
      }
      if (scope_index >= scopes.size()) {
        trace("bad scope index " << scope_index << ">=" << scopes.size()
          << " for " << kind << " " << name
        );
        return false;
      }
      scope = scopes[scope_index];
      assert(base::is_valid(scope));
    }
    in.ignore();
    //trace("create " << kind << " " << name << " @" << location);
    symbols.push_back(
      project->get_local_symbol(kind, name, location, scope)
    );
  }
  return true;
}
