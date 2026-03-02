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

#include "LinkerScriptAnalyzer.h"
#include "Project.h"
#include "Linker.h"
#include "GlobalSymbol.h"
#include "LocalSymbol.h"
#include "Hdir.h"
#include "base/filesystem.h"
#include "base/string_util.h"
#include "base/os.h"
#include <string>
#include <sstream>

// Linker Script documentation:
// https://sourceware.org/binutils/docs/ld/Scripts.html
// https://home.cs.colorado.edu/~main/cs1300/doc/gnu/ld_3.html#SEC48
//
// LLVM source code is in llvm/lld/ELF, docs in llvm/lld/docs/NewLLD.rst
//
// Both memory sections and memory regions are ELF compile- and link-time only
// concepts.  The executable running on an MCU just contains code and data.
// 
// A *section* is part of an object or executable file; typical examples
// are .bss .text and .data. It contains code and/or data.
//
// A memory *region* is an area in the memory address space with specific
// properties. Examples are flash and RAM, or internal and external RAM,
// depending on the target architecture. A memory region can contain data from
// multiple sections.  Memory regions are defined in the linkerscript, to help
// the linker to put all code and data at an appropriate address.
//
// The linker will combine sections from input files into sections in the output
// file, and then assign output sections to memory regions, under control of a
// linker script. An output sections may have a VMA (virtual memory address)
// where it resides while the program is running and a possibly different LMA
// (load memory address) from where it should be loaded at startup. In a bare
// metal application, code to copy data from LMA (typically in flash) to VMA
// (typically in RAM) is typically included in the executable. Alternatively, a
// separate boot loader can be used.
//
// The `size` command only considers LMA and sums up the sizes in three
// categories.
//
//
// Linker command line:
// --------------------
//
//   - symbols can be defined even before the linkerscript is started by using
//     for example "--defsym=FOO=0x1234" or equivalently "--defsym FOO 0x1234"
//     on the linker command line.
//
//   - the entry point can be defined using "-e name" or "--entry=name" where
//     name is a symbol name or an address.  An entry point on the command line
//     has priority over an entry point in the linker script.
//
//   - an undefined symbol can be added using "-u name"
//
//   - for debugging,  -M generates a map file
//
//   - -m<emulation-kind> sets the emulation kind to be used by the linker.
//     Accepted values include -marmelf and -melf32lriscv; see
//     llvm/lld/ELF/Driver.cpp in 'parseEmulation(...)' or search for 'ekind'.
//     This is required when running the linker without object files.  It is
//     used in linkerMain to select a template instance for `link<ELFT>(args)`.
//     You can also set ekind directly to one of ELF32LE, ELF32BE, ELF64LE or
//     ELF64BE.
//
//     Alternatively, generate a dummy object file with the selected toolchain's
//     compiler and add it to the command line. Compiling an empty C file is
//     fine:
//       echo "" > _.c
//       ...arm-none-eabi-gcc _.c -c
//       ld.lld _.o ...
//
//
// Linker script syntax:
//----------------------
//
// script = (command (';' | EOL))*
//
// command = keyword arg* | assignment
//
// assignment
// # . = 0x10000;
//
// name = normal-char * | '"' non-double-quote-char* '"'
//
// normal-char:  [A-Za-z_][A-Za-z_.-]*
//
// Comment: /* ... */ treated like white space
//
// All commands are top-level only unless stated below
//
// ENTRY(symbol)            // in top-level or SECTIONS
// INCLUDE filename         // in top-level, MEMORY, SECTIONS or output section
// INPUT(file, file, ...)   // file can also be AS_NEEDED(file file ...)
// INPUT(file file ...)     // file can also be AS_NEEDED(file file ...)
// GROUP(file, file, ...)   // file can also be AS_NEEDED(file file ...)
// GROUP(file file ...)     // file can also be AS_NEEDED(file file ...)
// OUTPUT(filename)
// SEARCH_DIR(path)
// STARTUP(filename)
// OUTPUT_FORMAT(bfdname)
// OUTPUT_FORMAT(default, big, little)
// TARGET(bfdname)
// REGION_ALIAS(alias, region)
// ASSERT(expression, message) // expression can be more than a name
// EXTERN(symbol symbol ...)
// FORCE_COMMON_ALLOCATION
// INHIBIT_COMMON_ALLOCATION
// FORCE_GROUP_ALLOCATION
// INSERT AFTER output_section
// INSERT BEFORE output_section
// NOCROSSREFS(section section ...)
// NOCROSSREFS_TO(section section ...)
// OUTPUT_ARCH(bfdarch)
// LD_FEATURE(string)
//
// symbol = expression ;
// symbol += expression ;
// symbol -= expression ;
// symbol *= expression ;
// symbol /= expression ;
// symbol <<= expression ;
// symbol >>= expression ;
// symbol &= expression ;
// symbol |= expression ;
//
// # The semicolon after expression is required.
// # Assignments are allowed at three locations: top, in SECTIONS, in a section
// # description in SECTIONS. Example:
// #   floating_point = 0;
// #   SECTIONS
// #   {
// #     .text :
// #       {
// #         *(.text)
// #         _etext = .;
// #       }
// #     _bdata = (. + 3) & ~ 4;
// #     .data : { *(.data) }
// #   }
// #
// # PROVIDE, HIDDEN and PROVIDE_HIDDEN are allowed anywhere where an assignment
// # is allowed.
//
// PROVIDE(symbol = expression)
// HIDDEN(symbol = expression)
// PROVIDE_HIDDEN(symbol = expression)
//
// KEEP(section)
// SECTIONS {
//   sections-command
//   sections-command
//   ...
// }
// #  Each sections-command may of be one of the following:
// #    - an ENTRY command (see section Setting the entry point)
// #    - a symbol assignment (see section Assigning Values to Symbols)
// #    - an output section description
// #    - an overlay description
//
// # Output section description:
//
//   section [address] [(type)] :
//     [AT(lma)]
//     [ALIGN(section_align) | ALIGN_WITH_INPUT]
//     [SUBALIGN(subsection_align)]
//     [constraint]
//   {
//     output-section-command
//     output-section-command
//     ...
//   } [>region] ['AT>' region-name] [':' (phdr | 'NONE')]* [=fillexp] [,]
//
// # The whitespace around section is required, so that the section name is
// # unambiguous. The colon and the curly braces are also required. The line
// # breaks and other white space are optional.
//
// # 'section' is the output section name:
// #    - in a.out, limited to .text or .data or .bss
// #    - special keyword /DISCARD/
//
// # The linker automatically PROVIDEs __start_<section> and __stop_<section> if
// # section name is a C identifier, e.g. does not start with a '.' like most
// # section names do.
//
// # 'address' is an expression.
//
// # (sub-)section_align?
//
// # constraint?
//
// #  Each output-section-command may be one of the following:
// #    - a symbol assignment
// #    - an input section description
// #    - data values to include directly BYTE SHORT ...
// #    - a special output section keyword e.g. the obsolete '[COMMON]'
//
// input-section-description =
//     [exclude-filename-list] filename-spec
//     ['('
//        (section-name-wildcard
//        |exclude-filename-list
//        |input-section-flags)*
//    
//     ')']
//     | 'KEEP' '(' input-section-description ')'
//
// exclude-filename-list = 'EXCLUDE_FILE' '(' filename-spec* ')'
//
// exclude-section-list = 'EXCLUDE_FILE' '(' section-name-wildcard ')'
//
// input-section-flags = 'INPUT_SECTION_FLAGS' '(' section-flags-expression ')'
//
// section-flags-expression =
//      section-flag
//      | section-flags-expression '&' section-flags-expression
//      | '!' section-flags-expression
//
// section-flag = 'SHF_MERGE' | 'SHF_STRINGS' | 'SHF_WRITE' | ...
//
// filename-spec = filename-wc | archive-wc ':' [filename-wc] | ':' filename-wc
//
// archive-wc = filename-wc
//
// filename-wc = ... can contain shell wildcards '*' '?' '[...]' ('\' anychar)
//                   only matches files from command line or INPUT statement
//              | 'SORT' '(' filename-wc* ')'
//              | 'SORT_BY_NAME' '(' filename-wc* ')'
//              | 'SORT_BY_ALIGNMENT' '(' filename-wc* ')'
//              | 'SORT_BY_INIT_PROPERTY' '(' filename-wc* ')'
//              | 'SORT_NONE' '(' filename-wc* ')'
//              (sorting can be nested!)
//
// section-name-wildcard = ... | 'COMMON' | 'KEEP' '('
//
// # Data values to include directly:
// 
// data-command =
//    ('BYTE' | 'SHORT' | 'LONG' | 'QUAD' | 'SQUAD' | 'FILL') '(' expression ')'
// QUAD is zero-extended and SQUAD is signed extended when both host and target
// are 32 bits
//
// # Output section keywrds
//
// output-section-keyword = 'CREATE_OBJECT_SYMBOLS' | 'CONSTRUCTORS'
//
// section-type = 'NOLOAD' | 'READONLY' | ... | 'TYPE' '=' type
//                | 'READONLY '(' 'TYPE' '=' type ')'
//
// type = integer | 'SHT_PROGBITS' | 'SHT_STRTAB' | ...
//   SHT_NOTE, SHT_NOBITS, SHT_INIT_ARRAY, SHT_FINI_ARRAY, and SHT_PREINIT_ARRAY
//
// lma = expression
//
// phdr = name
//
// fillexpr = expression
//
// PHDRS '{'
//    ( name type ['FILEHDR'] ['PHDRS'] ['AT' '(' address ')']
//          ['FLAGS' '(' flags ')'] )*
// '}'
//
// OVERLAY [start] ':' ['NOCROSSREFS'] ['AT' '(' ldaddr')'] '{'
//     (section-name '{' output-section-command '}' )*
// '}' ['>' region] [':' phdr ...] ['=' fillexpr] [ ',' ]
//
//
// start = expression
// ldaddr = expression
//
// MEMORY {
//    ( name ['(' attr ')'] ':' ('ORIGIN' | 'LENGTH') '=' expression )*
// }
//
// attr = ('R' | 'W' | 'X' | 'A' | 'I' | 'L' | '!'
//
// VERSION '{'
//  
// Expressions:
// .   (only within SECTIONS command)

// Experimentally determined: all expressions are unsigned (e.g. >> shifts 0
// bits into the value, not sign bits,  and -1 > 0
//

sa::LinkerScriptAnalyzer::LinkerScriptAnalyzer(Project *project)
  : Process("@linker-script-analyzer")
  , OriginalExternalAnalyzer(project->get_file(""))
  , project(project)
{
  trace("Create linker script analyzer " << (void*)this);
}

sa::LinkerScriptAnalyzer::~LinkerScriptAnalyzer()
{
  trace("Destroy linker script analyzer " << (void*)this);
  assert_(is_up_to_date(), process_name());
  for (auto diagnostic: diagnostics) {
    trace("remove linker script " << *diagnostic);
    diagnostic->exclude_instance();
  }
  for (auto occurrence: occurrences) {
    trace("remove linker script " << *occurrence);
    occurrence->remove_instance(true);
  }
}

void sa::LinkerScriptAnalyzer::set_args(
  const std::vector<std::string>& script_analysis_args
)
{
  trace("Set linker script analyzer args " << script_analysis_args);
  command.clear();
  command.push_back(project->get_linker_script_analyzer_path());
  command.push_back("-flavor");
  command.push_back("gnu");
  for (size_t i = 0; i < script_analysis_args.size(); ++i) {
    command.push_back(script_analysis_args[i]);
  }
  trigger();
}

void sa::LinkerScriptAnalyzer::on_out_of_date()
{
  trace(*this << " out of date: block linker");
  grab();
  project->linker->block();
  project->linker->trigger();
}

void sa::LinkerScriptAnalyzer::on_up_to_date()
{
  trace(*this << " up to date: unblock linker");
  project->linker->unblock();
  drop();
}

void sa::LinkerScriptAnalyzer::grab()
{
  increment_ref_count();
}

void sa::LinkerScriptAnalyzer::drop()
{
  Lock lock(project);
  decrement_ref_count();
}

static bool is_known_option_to_lld(const std::string &arg)
{
  // Implementation below is a temporary hack.  Should we check the real list of
  // options known to lld?  An easy way to obtain that is is to run lld --help.
  return //!base::begins_with(arg,"--warn-")||
    arg != "--warn-section-align" &&
    arg != "--print-memory-usage" ;
}

void sa::LinkerScriptAnalyzer::run()
{
  trace_nest("Run linker script analysis");
  std::string build_path;
  {
    Project::Lock lock(project);
    build_path = project->get_build_path();
  }
  std::vector<const char*> args;
  for (auto const &arg: command) {
    if (is_known_option_to_lld(arg)) {
      args.push_back(arg.data());
    }
  }
  args.push_back(0);
  trace(
    "\nscript lld command: ( cd "
    << base::quote_command_arg(build_path) << " && "
    << base::os::quote_command_line(args.data()) << " )\n"
  );
  OriginalExternalAnalyzer::run(args.data(), build_path.data());
  
  // Apply results
  Project::Lock lock(project);
  if (build_path != project->get_build_path()) {
    _cancel();
  }
  if (!cancelled()) {
    for (auto file: new_linker_scripts) {
      trace("used as linkerscript: " << file);
      file->inc_inclusion_count("used as linkerscript");
    }
    for (auto occurrence: new_occurrences) {
      trace("insert linker script " << *occurrence);
      occurrence->insert_instance(true);
    }
    for (auto const &[name, region]: new_memory_regions) {
      auto it = memory_regions.find(name);
      if (it == memory_regions.end()
        || it->second.origin != region.origin
        || it->second.size != region.size
      ) {
        trace("report memory region " << name << " " << region.origin
          << " " << region.size
        );
        project->set_memory_region(name, true, region.origin, region.size);
      }
    }
    for (auto const &[name, region]: memory_regions) {
      auto it = new_memory_regions.find(name);
      if (it == new_memory_regions.end()) {
        trace("report memory region " << name << " removed");
        project->set_memory_region(name, false, 0, 0);
      }
    }
    for (auto const &[name, section]: new_memory_sections) {
      auto it = memory_sections.find(name);
      if (it == memory_sections.end()
        || it->second.runtime_region != section.runtime_region
        || it->second.load_region != section.load_region
      ) {
        trace("report memory section " << name << " " << section.runtime_region
          << " " << section.load_region
        );
        project->set_memory_section(name, true, section.runtime_region,
          section.load_region
        );
      }
    }
    for (auto const &[name, section]: memory_sections) {
      auto it = new_memory_sections.find(name);
      if (it == new_memory_sections.end()) {
        trace("report memory section " << name << " removed");
        project->set_memory_section(name, false, "", "");
      }
    }
    trace("currently " << diagnostics.size() << " ext diagnostics");
    trace("add " << new_diagnostics.size() << " ext diagnostics");
    project->add_unit_diagnostics(new_diagnostics);
    new_occurrences.swap(occurrences);
    new_diagnostics.swap(diagnostics);
    new_linker_scripts.swap(linker_scripts);
    new_memory_regions.swap(memory_regions);
    new_memory_sections.swap(memory_sections);
    
    trace("remove " << new_diagnostics.size() << " ext diagnostics");
    project->remove_unit_diagnostics(new_diagnostics);
    trace("currently " << diagnostics.size() << " ext diagnostics");
    for (auto occurrence: new_occurrences) {
      trace("remove linker script " << *occurrence);
      occurrence->remove_instance(true);
    }
    for (auto file: new_linker_scripts) {
      file->dec_inclusion_count("used as linkerscript");
    }
  }
  // Clear lists while project is still locked
  new_occurrences.clear();
  new_diagnostics.clear();
  new_linker_scripts.clear();
  new_memory_regions.clear();
  new_memory_sections.clear();
}

bool sa::LinkerScriptAnalyzer::is_linker_script(base::ptr<File> file)
{
  return linker_scripts.find(file) != linker_scripts.end();
}

void sa::LinkerScriptAnalyzer::handle_stderr(std::istream &in)
{
  trace_nest("processing linker script stderr");
  std::string line;
  while (std::getline(in, line)) {
    debug_atomic_writeln("lld stderr: " << line);
  }
}

base::ptr<sa::File> sa::LinkerScriptAnalyzer::get_file(const std::string &path)
{
  Project::Lock lock(project);
  base::ptr<sa::File> file = project->get_file(path);
  new_linker_scripts.insert(file);
  return file;
}

sa::Section *sa::LinkerScriptAnalyzer::create_section(
  const std::string &name,
  const std::string &member_name
)
{
  // Section is not used in linker scripts
  (void)name;
  (void)member_name;
  return 0;
}

base::ptr<sa::Symbol> sa::LinkerScriptAnalyzer::get_global_symbol(
  const std::string &link_name
)
{
  return project->get_global_symbol(link_name);
}

base::ptr<sa::Symbol> sa::LinkerScriptAnalyzer::get_local_symbol(
  EntityKind kind,
  const std::string &user_name,
  const FileLocation &ref_location
)
{
  return project->get_local_symbol(kind, user_name, ref_location, 0);
}

void sa::LinkerScriptAnalyzer::add_include(
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
  new_occurrences.push_back(occurrence);
  trace("add include " << *occurrence);
}

sa::Occurrence *sa::LinkerScriptAnalyzer::add_occurrence(
  base::ptr<Symbol> const &symbol,
  EntityKind ekind,
  OccurrenceKind okind,
  Section *section,
  base::ptr<File> const &file,
  Range range
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
      assert_(ekind == symbol->kind, ekind << " " << symbol->kind);
  }
  return add_occurrence(symbol, okind, style, section, file, range);
}

sa::Occurrence *sa::LinkerScriptAnalyzer::add_occurrence(
  base::ptr<Symbol> const &symbol,
  OccurrenceKind kind,
  OccurrenceStyle style,
  Section *section,
  base::ptr<File> const &file,
  Range range
)
{
  // Section is used in Analyzer::add_occurrence for inline function
  // declarations. These do not occur in linker scripts.
  (void)section;
  auto occurrence = file->get_occurrence(
    kind, style, symbol, range
  );
  trace("add linkerscript occurrence " << *occurrence);
  new_occurrences.push_back(occurrence);
  return occurrence;
}

void sa::LinkerScriptAnalyzer::open_scope()
{
  // not implemented nor used
  assert(false);
}

void sa::LinkerScriptAnalyzer::close_scope()
{
  // not implemented nor used
  assert(false);
}

void sa::LinkerScriptAnalyzer::report_diagnostic(
  const std::string &message,
  Severity severity,
  base::ptr<File> const &file,
  Location location
)
{
  if (base::begins_with(message, "undefined symbol: ")) {
    // Undefined symbols are to be expected, because we link some random object
    // files from the toolchain and no application files. Just ignore them for
    // now.  We need at least one object file to run the linker script analyzer
    // (lld), so to avoid these errors, we would need to remove any toolchain
    // objects. The lld command will not work without object files, so we would
    // have to add an empty object file created by the right toolchain.
    return;
  }
  project->lock();
  trace_nest("Diagnostic: " << message);
  trace("Report " << severity << ": " << message << " at "
    << (file ? file->get_name() : "<no-file>")
    << "." << location
  );
  auto diagnostic = file ? file->get_diagnostic(message, severity, location)
    : project->get_diagnostic(message, severity);
  project->unlock();
  new_diagnostics.push_back(diagnostic);
}

void sa::LinkerScriptAnalyzer::report_missing_header(const std::string &name)
{
  // not implemented yet
  (void)name;
}

void sa::LinkerScriptAnalyzer::add_memory_region(
  const std::string &name,
  size_t origin,
  size_t size
)
{
  trace("add memory region " << name << " " << origin << " " << size);
  new_memory_regions[name] = MemoryRegion({origin, size});
}

void sa::LinkerScriptAnalyzer::add_memory_section(
  const std::string &name,
  const std::string &runtime_region,
  const std::string &load_region
)
{
  trace("add memory section " << name << " " << runtime_region << " "
    << load_region
  );
  new_memory_sections[name] = MemorySection({runtime_region, load_region});
}

#ifdef SELFTEST

using namespace sa;

static void project_status_callback(
  Project *project,
  ProjectStatus status
)
{
  (void)project;
  (void)status;
}

static void inclusion_status_callback(
  const char *path,
  void *user_data,
  bool linked,
  bool included
)
{
  (void)path;
  (void)user_data;
  (void)linked;
  (void)included;
}

static void analysis_status_callback(
  const char *path,
  void *user_data,
  AnalysisStatus old_status,
  AnalysisStatus new_status
)
{
  (void)path;
  (void)user_data;
  (void)old_status;
  (void)new_status;
}

static void update_flags_callback(
  Unit *analyzer,
  const char *path,
  void *user_data
)
{
  (void)analyzer;
  (void)path;
  (void)user_data;
}

static void *add_symbol_callback(
  const char *name, EntityKind kind, Symbol *symbol
)
{
  (void)name;
  (void)kind;
  (void)symbol;
  return 0;
}

static void drop_symbol_callback(void *symbol_user_data)
{
  (void)symbol_user_data;
}

static void linker_status_callback(
  Project *project,
  LinkerStatus status
)
{
  (void)project;
  (void)status;
}

static void hdir_usage_callback(const char *path, InclusionStatus status)
{
  (void)path;
  (void)status;
}

static void *add_diagnostic_callback(
  const char *message,
  Severity severity,
  const char *path,
  void *file_user_data,
  unsigned offset,
  void *after_user_data
)
{
  (void)message;
  (void)severity;
  (void)path;
  (void)file_user_data;
  (void)offset;
  (void)after_user_data;
  return 0;
}

static void remove_diagnostic_callback(void *user_data)
{
  (void)user_data;
}

static void more_diagnostics_callback(
  Project *project,
  Severity severity,
  unsigned number_of_unreported_diagnostics
)
{
  (void)project;
  (void)severity;
  (void)number_of_unreported_diagnostics;
}

static void *add_occurrence_in_file_callback(
  const char *path,
  unsigned offset,
  unsigned end_offset,
  OccurrenceKind kind,
  void *entity_user_data,
  Entity *entity,
  void *scope_user_data
)
{
  (void)path;
  (void)offset;
  (void)end_offset;
  (void)kind;
  (void)entity_user_data;
  (void)entity;
  (void)scope_user_data;
  return 0;
}

static void remove_occurrence_in_file_callback(void *occurrence_user_data)
{
  (void)occurrence_user_data;
}

static void *add_occurrence_of_entity_callback(
  const char *path,
  unsigned offset,
  unsigned end_offset,
  OccurrenceKind kind,
  void *entity_user_data,
  Entity *entity
)
{
  (void)path;
  (void)offset;
  (void)end_offset;
  (void)kind;
  (void)entity_user_data;
  (void)entity;
  return 0;
}

static void remove_occurrence_of_entity_callback(void *occurrence_user_data)
{
  (void)occurrence_user_data;
}

static void report_internal_error_callback(
  const char *message,
  void *user_data
)
{
  (void)message;
  (void)user_data;
}

static Project *create_dummy_project()
{
  return new Project(
    "project_path",
    "cache_path",
    "resource_path",
    "lib_path",
    project_status_callback,
    inclusion_status_callback,
    analysis_status_callback,
    update_flags_callback,
    add_symbol_callback,
    drop_symbol_callback,
    linker_status_callback,
    hdir_usage_callback,
    add_diagnostic_callback,
    remove_diagnostic_callback,
    more_diagnostics_callback,
    add_occurrence_in_file_callback,
    remove_occurrence_in_file_callback,
    add_occurrence_of_entity_callback,
    remove_occurrence_of_entity_callback,
    report_internal_error_callback,
    0
  );
}

int main(int argc, char *argv[])
{
  assert(argc == 2);
  test_out(argv[1]);
  Project *project = create_dummy_project();
  project->lock();
  base::ptr<File> file = project->get_file(argv[1]);
  project->unlock();
  LinkerScriptAnalyzer analyzer(file);
  analyzer.run();
  test_out("occurrences: {");
  for (auto occurrence: analyzer.get_occurrences()) {
    test_out(*occurrence);
  }
  test_out("}");
  project->lock();
  return 0;
}

#endif
