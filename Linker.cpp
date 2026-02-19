// Copyright 2018-2024 Johan Cockx
#include "Linker.h"
#include "Project.h"
#include "File.h"
#include "GlobalSymbol.h"
#include "Section.h"
#include "Unit.h"
#include "LinkerScriptAnalyzer.h"
#include "LinkCommandAnalyzer.h"
#include "base/filesystem.h"
#include "base/string_util.h"
#include "base/os.h"
#include "base/platform.h"
#include <string>
#include <string.h>
#include <set>
#include <algorithm>
#include <stdio.h>
#include "base/debug.h"

namespace {
  // strchrnul does not exist in msys2?
  inline char *strchrnul(const char *s, int c)
  {
    char *p = strchr((char*)s,c);
    return p ? p : (char*)s + strlen(s);
  }
}

sa::Linker::Linker(Project *project): Process("@linker"), project(project)
{
  trace("create Linker " << this);
  set_urgent(true);
  // Above call also does lock/unlock so releases initial values to other
  // threads; no need to do it again.
  //Lock lock(this);
}

sa::Linker::~Linker()
{
  trace("destroy Linker " << this);
  assert_(is_up_to_date(), process_name());
  if (_linker_script_analyzer) {
    delete _linker_script_analyzer;
  }
}

void sa::Linker::_add_forced_file(File *file)
{
  trace_nest("Linker: add forced file " << *file);
  assert(is_locked());
  assert(base::is_valid(file));
  _forced_files.insert(file);
  for (auto section: file->get_sections()) {
    assert(base::is_valid(section));
    linked_sections.insert(section);
    section->set_linked();
  }
}

void sa::Linker::_remove_forced_file(File *file)
{
  trace_nest("Linker: remove forced file " << *file);
  assert(is_locked());
  assert(base::is_valid(file));
  _forced_files.erase(file);
  // Effect on sections is unknown at this point, will be determined during the
  // next link.
}

void sa::Linker::drop_section(Section *section)
{
  assert(base::is_valid(section));
  trace_nest("drop section " << *section);
  assert(project->is_locked());
  assert(section->is_linked());
  linked_sections.erase(section);
  section->set_unlinked();
}

void sa::Linker::reload_as_linker_script(File *file)
{
  assert(project->is_locked());
  if (_linker_script_analyzer &&
    _linker_script_analyzer->is_linker_script(file)
  ) {
    trace("reload linker script " << *file);
    _linker_script_analyzer->trigger();
  }
}

void sa::Linker::set_link_command(std::vector<std::string> command)
{
  trace_nest("Linker: set link command");
  trace("command: " << base::quote_command_line(command));

  assert(!command.empty());
  trace_atomic_code(
    std::cerr << "\nlink command:\n";
    for (auto arg: command) {
      std::cerr << " '" << arg << "'";
    }
    std::cerr << "\n\n";
  );
  Project::Lock lock(project);
  LinkCommandAnalyzer link_command_analyzer(
    command,
    project->get_build_path(),
    project->get_cache_path() // TODO: choose temp path outside project
  );

  std::vector<base::ptr<Diagnostic>> diagnostics;
  if (!link_command_analyzer.diagnostic_message.empty()) {
    trace("Could not analyze link command");
    diagnostics.push_back(project->get_diagnostic(
        link_command_analyzer.diagnostic_message, Severity_fatal)
    );
  } else {
    trace("Linker: examine link file args");
    std::vector<base::ptr<File>> files;
    bool in_group = false;
    for (auto path: link_command_analyzer.file_args) {
      trace_nest("command line file: " << path);
      if (path == "(") {
        if (!in_group) {
          in_group = true;
          files.emplace_back((File*)0);
        }
      } else if (path == ")") {
        if (in_group) {
          in_group = false;
          files.emplace_back((File*)0);
        }
      } else {
        auto abs_path = base::get_normalized_path(
          base::get_absolute_path(path, project->get_build_path())
        );
        trace("abspath " << abs_path);
        base::ptr<File> file = project->get_file(abs_path);
        assert_(file->has_linkable_file_kind(), file->get_name() << " "
          << file->file_kind
        );
        trace("add " << *file);
        files.emplace_back(file);
        file->inc_in_link_command_count();
      }
    }
    _command_files.swap(files);
    for (auto file: files) {
      if (file) {
        assert(file->has_linkable_file_kind());
        file->dec_in_link_command_count();
      }
    }
    trace("Linker: process symbols defined on the link command line");
    for (auto name: link_command_analyzer.defsyms) {
      defsyms.insert(name);
    }
    trace("Linker: analyze linker script");
    if (!_linker_script_analyzer) {
      _linker_script_analyzer = new LinkerScriptAnalyzer(project);
    }
    // set_args also triggers the linker script analyzer, which blocks the
    // linker until the linker script analyzer finishes.
    _linker_script_analyzer->set_args(
      link_command_analyzer.script_analysis_args
    );
    garbage_collect_sections = link_command_analyzer.garbage_collect_sections;
    trigger();
  }
  project->add_unit_diagnostics(diagnostics);
  diagnostics.swap(link_command_diagnostics);
  project->remove_unit_diagnostics(diagnostics);
  diagnostics.clear();
}

void sa::Linker::run()
{
  trace_nest("execute linker");
  assert(todo.empty());
  assert(resolved.empty());
  assert(status_map.empty());
  
  // Keep project locked while linking, so that files, sections and
  // occurrences cannot disappear.
  //
  // To avoid deadlocks when both the project and the linker are locked, always
  // lock the project before locking the linker.
  Lock project_lock(project);

  // Get entry points for linker scripts.
  if (_linker_script_analyzer) {
    for (auto occ: _linker_script_analyzer->get_occurrences()) {
      if (occ->kind == OccurrenceKind_use && occ->entity->is_global_symbol()) {
        trace("require " << *occ);
        require(occ->entity->get_name().data());
      }
    }
  }

#if 1
  // Assume the main function is the entry point of the program.
  base::ptr<GlobalSymbol> main = project->_get_global_symbol("main");
  require(main->link_name.data());
#endif
  
  // Keep the linker locked while running, so that forced files cannot be added
  // or removed while it is running.
  Lock lock(this);
  std::set<Section*> old_linked_sections;
  linked_sections.swap(old_linked_sections);
  LinkerStatus old_linker_status = _linker_status;
  assert(linked_sections.empty());

  // Initialize linker status to done;  change that when an error is detected.
  _linker_status = sa::LinkerStatus_done;

  // Handle force-included and fixed files.
  //
  // When garbage-collect-sections is active, sections in force-included and
  // fixed files are treated like sections in automatic files: they are only
  // included when one of the defined symbols is required. Otherwise, they are
  // included unconditionally below.
  //
  // This garbage collection algorithm is not entirely compatible with the
  // behavior of the linker.  The linker initially inserts files mentioned on
  // the command line, and performs garbage-collection at the end, after
  // linking. As a result, a symbol used in a file mentioned on the command line
  // can be resolved by any archive.  In our algorithm, however, insertions of
  // the file is postponed until one of its symbols is used.  If the use is in a
  // later archive, uses cannot be resolved against earlier archives.
  //
  // Performing garbage collection at the end requires us to keep information
  // about which section requires which other sections, which we currently don't
  // do. To be reconsidered later.
  //
  if (!garbage_collect_sections) {
    std::set<base::ptr<File>> files = _forced_files;
    files.insert(_command_files.begin(), _command_files.end());
    for (auto file: files) {
      if (!file) continue;
      assert(file->has_linkable_file_kind());
      if (file->file_kind != FileKind_archive) {
        trace_nest("force " << *file
          << " forced=" << (_forced_files.find(file) != _forced_files.end())
          << " in-command=" << file->is_in_link_command()
        );
        for (auto section: file->get_sections()) {
          assert(base::is_valid(section));
          add_section(section);
          //
          // When a file is force-included, either through the filetree or from
          // the linker command line - make sure that for the defined symbols,
          // any alternatives in automatic files are also considered.
          //
          // For example, if a weak definition is force-included, a strong
          // definition in an automatic file should be considered too.
          //
          // If a strong definition is force-included, alternative strong
          // definitions in automatic files should still be considered;
          // otherwise, force-including one file may exclude automatic files
          // defining the same symbol, which feels weird.
          //
          // We do this only for force-included files, not for automatic
          // files. An automatic file may get included for a global symbol A,
          // and may contain another global symbol B that is not used but gets
          // included as a side-effect, because it is in the same section as A.
          // In this case, there is no reason to include alternative definitions
          // of B as well, unless B is required elsewhere in the code.
          //
          require_section_defs(section);
        }
      }
    }
  }
  
  // Link object files.
  link();

  // Link archives one by one, mimicking gcc behavior.
  bool in_group = false;
  size_t progress = 0;
  for (size_t i = 0; i < _command_files.size(); ++i) {
    if (todo.empty()) break;
    auto file = _command_files[i];
    assert(base::is_valid_or_null(file));
    if (!file) {
      if (!in_group) {
        // Enter group; keep track of progress
        in_group = true;
        progress = linked_sections.size();
        trace("enter group, progress = " << progress);
      } else {
        // Check progress in group
        size_t new_progress = linked_sections.size();
        if (new_progress > progress) {
          // Progressing, so repeat group
          progress = new_progress;
          while (_command_files[--i]);
          trace("repeat group, progress = " << progress);
        } else {
          // No progress, so leave group
          in_group = false;
          trace("leave group, progress = " << progress);
        }
      }
    } else if (file->file_kind == FileKind_archive) {
      trace("link " << file->get_name());
      assert(base::is_valid(file));
      link(file);
    }
  }

  //
  // Add linkerscript symbols
  if (_linker_script_analyzer) {
    trace_nest("add linker script symbols");
    for (auto def: _linker_script_analyzer->get_occurrences()) {
      if (def->is_defining()) {
        auto symbol = def->entity->as_global_symbol();
        LinkStatus &status = status_map[symbol->link_name.data()];
        if (def->kind == OccurrenceKind_weak_definition) {
          trace(symbol->link_name << " PROVIDE'd by linker script at " << *def);
          if (status < LinkStatus_weakly_defined) {
            status = LinkStatus_weakly_defined;
          }
        }
        if (def->kind == OccurrenceKind_definition) {
          trace(symbol->link_name << " defined by linker script at " << *def);
          // Defined in linker script.  Linker script definitions silently
          // override code definitions.
          if (status < LinkStatus_defined) {
            status = LinkStatus_defined;
          } else {
            trace("Definition of " << symbol->link_name
              << " silently overridden by linkerscript definition"
            );
            // TODO: give a warning?
          }
        }
      }
    }
  }

  for (auto name: todo) {
    LinkStatus &status = status_map[name];
    if (status == LinkStatus_undefined) {

      auto symbol = project->find_global_symbol(name);
      assert(symbol);
      if (defsyms.find(symbol->link_name) != defsyms.end()) {
        trace("defined on the command line: " << symbol->link_name);
        status = LinkStatus_weakly_defined;
        continue;
      }
      trace("Linker error because of " << name << " " << status);
      _linker_status = LinkerStatus_error;
    }
  }

  // Report results
  if (cancelled()) {
    trace("linker cancelled " << this);
    linked_sections.swap(old_linked_sections);
    _linker_status = old_linker_status;
  } else {
    {
      trace_nest("linked sections: " << linked_sections.size());
      for (auto section: linked_sections) {
        assert(base::is_valid(section));
        trace("linked section: " << *section);
        if (old_linked_sections.find(section) == old_linked_sections.end()) {
          trace_nest("set linked");
          section->set_linked();
        }
      }
    }
    {
      trace_nest("old linked sections: " << old_linked_sections.size());
      for (auto section: old_linked_sections) {
        assert(base::is_valid(section));
        if (linked_sections.find(section) == linked_sections.end()) {
          trace_nest("set unlinked section " << *section);
          section->set_unlinked();
        }
      }
    }
    for (auto const &[name, status]: status_map) {
      trace("Final link status for " << name << ": " << status);
      assert_(status != LinkStatus_none, name << ": " << status);
    }
    report_linker_status();
    project->set_linker_results(status_map, main);
  }

#if 1
  // Reset main pointer before the project is unlocked.  If there is no main
  // function, resetting or destroying the pointer will decrement the symbol's
  // reference count, potentially deleting the symbol, and the project must be
  // locked when that happens.
  main = 0;
#endif
  
  // Clear sets; these will be reused for the next run of the linker.
  todo.clear();
  status_map.clear();
  resolved.clear();
  defined.clear();
}

static bool is_builtin(const std::string &name)
{
  // Built-in functions usually start with __builtin_.  Sometimes, the same
  // function without the __builtin_ prefix is also built-in, but it doesn't
  // matter if we ignore that, because there is (or should be) a normal
  // implementation in one of the system libraries like libm.
  //
  // Unfortunately, the libm implementation of the AVR toolchain does not
  // contain an implementation for fabs (checked with
  // gnu_avr_toolchain_7.3.0_64b). The effect of this bug is that Embeetle
  // reports an undefined global 'fabs' where the actual linker usually doesn't.
  // If the application code takes the address of fabs, and the use of fabs is
  // not eliminated by link time optimisation (-flto), the actual linker also
  // issues an error.
  //
  // Gnu toolchains normally have a file
  // lib/gcc/avr/7.3.0/plugin/include/builtins.def that defines all builtins
  // using macros.  Preprocessing this C code will list all builtins with their
  // properties (remove _ after \ at end of lines):
  //
  // #define DEF_BUILTIN(ENUM, NAME, CLASS, TYPE, LIBTYPE, BOTH_P, FALLBACK_P, \_
  //   NONANSI_P, ATTRS, IMPLICIT, COND)                                       \_
  // ENUM, NAME, CLASS, TYPE, LIBTYPE, BOTH_P, FALLBACK_P, NONANSI_P, ATTRS,   \_
  // IMPLICIT, COND
  // #include "<toolchain-path>/lib/gcc/avr/7.3.0/plugin/include/builtins.def"
  //
  // BOTH_P is true iff the function without __builtin_ prefix is also builtin.
  //
  static std::string const prefix = "__builtin_";
  //
  // In some AVR projects, __builtin_avr_delay_cycles is declared in delay.h
  // without 'extern "C"', so the name is mangled.  The AVR linker still ignores
  // it.  For this reason, we need to check for the prefix in the *demangled*
  // name.
  if (base::begins_with(base::demangle(name), prefix)) {
    return true;
  }
  // TODO: use patch below only for Arduino toolchain. The Arduino toolchain's
  // libm does not implement fabs; it is only available as built-in.
  if (name == "fabs") {
    return true;
  }
  
  // TODO: temporary patch below. ITM_RxBuffer is declared in a header file and
  // only used in static inline functions in the same header file. If these
  // static inline functions are never called in the project, this should not
  // result in an undefined global.  As a temporary workaround, handle this as
  // if it is a builtin function. Note: could this be related to the fact that
  // we compile with --function-sections and link with --gc-sections, instead of
  // static inline? The function gets a separate section which is never used!
  if (name == "ITM_RxBuffer") {
    return true;
  }
  return false;
}

void sa::Linker::link(File *archive)
{
  trace_nest("link " << (archive ? archive->get_name() : "auto"));
  assert(project->is_locked());
  //
  //
  //
  // General rule: mimick gcc behavior as far as possible. In case of
  // ambiguities, try to generate an error or at least a warning.
  //
  // Details of gcc behavior, verified experimentally:
  //
  // 1. If the linker finds a weak definition in one library, it will *not*
  //    replace it with a strong definition in a later library.
  //
  // 2. If the linker has inserted a definition for a symbol in one library, and
  //    a reference to the same symbol is found in a later library, the
  //    definition from the first library is used even if the second library
  //    also has a definition.
  //
  // 3. If an archive defines both a strong and a weak version of a given
  //    symbol, it is not guaranteed that the strong version is linked in; this
  //    seems to depend on the order in which the versions were added to the
  //    archive. TODO: we should include both and/or give a warning in that
  //    case.
  //
  //
  // Repeatedly resolve symbols in the todo list, until no more symbols can be
  // resolved.
  //
  // To make the result independent of the order in which the symbols occur in
  // the todo-list, include all definitions if there is more than one. In the
  // case of strong definitions, this will result in multiply-defined global,
  // which is fine. It is up to the user to take a decision; this algorithm does
  // not have the necessary information to do that automatically.  For tentative
  // and weak definitions, it doesn't matter, because all tentative and weak
  // definitions are assumed to be equivalent.
  //
  // Link in two phases:
  //
  // 1. Resolve symbols using strong definitions only. This may pull in some
  //    weak definitions as a side effect.
  //
  // 2. If any symbols remain undefined, resolve them using tentative or weak
  //    definitions if possible.
  //
  // Linking in two phases is essential for C++ code with inline methods defined
  // in header files.  C++ inline methods are mapped to weak definitions,
  // especially if their address is taken. If we always immediately include all
  // weak definitions, then all includers of a header file with an inline method
  // will be included, regardless of whether the other definitions in that
  // section are needed.  This results in too much included code. The two phase
  // approach avoids that problem while maintaining order independence.
  //
  // A symbol can get defined because it is in the same section as the
  // definition of another symbol that is resolved. Before processing an
  // archive, all these symbols are moved to the resolved set, because gcc never
  // resolves a symbol that has already been resolved in an object file or
  // previous archive.
  for (auto name: defined) {
    trace("already defined -> mark resolved: " << name);
    resolved.insert(name);
  }
  defined.clear();
  //
  // List of required symbols that cannot be resolved in the current archive.
  // Once the todo-list is empty, the unresolved symbols will be swapped into
  // the todo-list before returning, so that they can be resolved in another
  // archive or by the linker script.
  std::vector<std::string> unresolved;
  //
  // Iterate until the todo-list is empty. Symbols will either be resolved or
  // added to the unresolved list. New symbols may be added to the todo-list
  // while iterating.
  trace("phase 1 todo: " << todo);
  while (!todo.empty()) {
    auto name = todo.back();
    todo.pop_back();
    LinkStatus &status = status_map[name];
    trace((archive ? archive->get_name() : "auto")
      << " phase 1: process " << name << " -> " << status
    );
    // We want to process this symbol even if it is already defined.  It could
    // be defined because another symbol in the same section was required.  In
    // that case, we still need to make sure that we look at all other
    // definitions of this symbol.
    if (status < LinkStatus_weakly_defined && is_builtin(name)) {
      trace(" `->built-in");
      status = LinkStatus_weakly_defined;
    }
    
    // When processing an archive, if there are any non-archive definitions of
    // any kind, prefer those over archive definitions.  This might include an
    // automatic file that was previously not selected, because the first use
    // was found in an archive.
    bool non_archive_def_available = false;
    if (archive) {
      trace("try non-archive definition first");
      resolve_global(name, OccurrenceKind_definition, 0);
      if (status < LinkStatus_defined) {
        resolve_global(name, OccurrenceKind_tentative_definition, 0);
        if (status < LinkStatus_tentatively_defined) {
          resolve_global(name, OccurrenceKind_weak_definition, 0);
        }
      }
      if (status >= LinkStatus_weakly_defined) {
        non_archive_def_available = true;
      }
    }
    if (!non_archive_def_available) {
      if (archive) {
        trace("now try archive version");
      }
      resolve_global(name, OccurrenceKind_definition, archive);
    }
    trace(" `-> " << status);
    if (status < LinkStatus_defined) {
      // Postpone further resolution to phase 2.
      unresolved.push_back(name);
    }
  }
  // Keep only symbols that are not even weakly defined yet. Weakly defined
  // symbols may be C++ methods; we don't want to pull in any extra definitions
  // of these methods, as explained above.
  for (auto name: unresolved) {
    LinkStatus &status = status_map[name];
    if (status < LinkStatus_weakly_defined) {
      trace("not yet defined after phase 1: " << name);
      todo.push_back(name);
    }
  }
  unresolved.clear();
  // Second phase.
  //
  // At the start of phase 2, the todo list does not contain any symbols with
  // strong definitions, because these symbols have been handled in phase 1.
  // However, new symbols with strong definitions may end up in the todo list
  // as a side-effect of including weak definitions, so we need to handle
  // strong definitions in phase 2 as well.
  trace("phase 2 todo: " << todo);
  while (!todo.empty()) {
    auto name = todo.back();
    todo.pop_back();
    LinkStatus &status = status_map[name];
    trace((archive ? archive->get_name() : "auto")
      << " phase 2: process " << name << " -> " << status
    );
    if (status < LinkStatus_weakly_defined && is_builtin(name)) {
      trace(" `->built-in");
      status = LinkStatus_weakly_defined;
    }
    // When processing an archive, if there are any non-archive definitions of
    // any kind, prefer those over archive definitions.  This might include an
    // automatic file that was previously not selected, because the first use
    // was found in an archive.
    bool non_archive_def_available = false;
    if (archive) {
      trace("try non-archive definition first");
      resolve_global(name, OccurrenceKind_definition, 0);
      if (status < LinkStatus_defined) {
        resolve_global(name, OccurrenceKind_tentative_definition, 0);
        if (status < LinkStatus_tentatively_defined) {
          resolve_global(name, OccurrenceKind_weak_definition, 0);
        }
      }
      if (status >= LinkStatus_weakly_defined) {
        non_archive_def_available = true;
      }
    }
    if (!non_archive_def_available) {
      if (archive) {
        trace("now try archive version");
      }
      resolve_global(name, OccurrenceKind_definition, archive);
      trace(" `-> " << status);
      if (status < LinkStatus_defined) {
        resolve_global(name, OccurrenceKind_tentative_definition, archive);
        if (status < LinkStatus_tentatively_defined) {
          resolve_global(name, OccurrenceKind_weak_definition, archive);
          if (status < LinkStatus_weakly_defined) {
            // Postpone further resolution to next archive.
            unresolved.push_back(name);
          }
        }
      }
    }
  }
  todo.swap(unresolved);
#ifndef NDEBUG
  for (auto name: todo) {
    LinkStatus &status = status_map[name];
    assert_(status < LinkStatus_weakly_defined, name);
    trace("todo: " << name << ": " << status);
  }
#endif
}

void sa::Linker::resolve_global(
  const std::string &name, sa::OccurrenceKind okind, File *archive
)
{
  // Below, we call add_sections for definitions with okind of the symbol,
  // including those in header files and excluded files.
  //
  // Visiting definitions in header files is the right thing to do, because
  // header files can be #included in included sections. Inside add_sections,
  // definitions only #included from excluded compilation units are skipped.
  //
  // Excluded files should be treated like header files, because they can also
  // be #included. 
  //
  // Including all definitions is the right thing to do, to avoid
  // undeterministic behavior depending on the order in which symbols are
  // resolved.
  //
  auto symbol = project->find_global_symbol(name);
  if (symbol) {
    trace_nest("try resolve " << *symbol << " with " << okind);
    for (auto const &definition: symbol->get_occurrences(okind)) {
      trace("for " << name << " try " << *definition); 
      add_sections(definition, archive);
    }
  } else {
    trace("cannot resolve " << name << ": not a global symbol");
  }
}

// Return true iff sections of the given file are allowed to be linked in in the
// linking step identified by 'archive': null in the first step, points to a
// specific archive mentioned on the command line in later steps.
//
// Linking is done in steps:
//
//  - first all files except for archives mentioned on the command line,
//
//  - then one by one all archives mentioned on the command line, in the order
//    in which they ae mentioned.
//
// In the first step, 'archive' is null.  In the following steps, 'archive' is
// the archive currently being linked.
//
bool sa::Linker::allow_file(File *file, File *archive)
{
  // Unconditionally allow the archive being processed in the current step.
  // Also allow any link candidate that is not a command-line archive.
  //
  // Link candidates are force-included files, non-object automatic files and
  // command line files. Automatic object files are not considered because it is
  // too tricky to recognize and avoid object files compiled from source files
  // in the project.
  //
  // Command-line archives are processed one by one in separate steps.
  return file == archive || (
    (file->is_link_candidate() || file->is_in_link_command())
    &&
    !(file->file_kind == FileKind_archive && file->is_in_link_command())
  );
}

// Add sections containing the given definition.  If archive is non-null, add
// only sections from that archive.  If it is null, add only sections from
// non-archives and force-included or automatic archives.
void sa::Linker::add_sections(
  base::ptr<sa::Occurrence> definition, File *archive
)
{
  trace_nest("try to add sections for " << *definition);
  assert(project->is_locked());
  base::ptr<File> file = definition->file;
  while (!file->has_linkable_file_kind() && file->get_includers().size() == 1) {
    file = file->get_includers().at(0)->file;
    trace("go to " << file->get_name());
  }
  
  if (file->get_includers().empty()) {
    // The definition is instantiated in exactly one file: it must be
    // instantiated at least once (or it wouldn't exist), and there are no
    // includers so it cannot be instiated in any includer.
    //
    // This is by far the most common case, so it is important to optimize.
    trace("no includers -> optimize");
    if (allow_file(file, archive)) {
      trace("file allowed");
      auto sections = file->get_sections();
      if (sections.size() == 1) {
        // Optimization: if there is only one section, it must include the
        // definition; where else could the definition be located? So no need to
        // further.
        auto section = sections.front();
        assert(base::is_valid(section));
        trace_nest("include " << *section << " to resolve " << *definition);
        add_section(section, definition);
      } else {
        add_sections_instantiating(definition, file);
      }
    } else {
      if (archive) {
        trace("not allowed: linking different archive only");
      } else {
        trace("not allowed, mode=" << file->get_mode());
      }
    }
  } else {
    // The definition may be instantiated in more than one file.  We need to
    // check includers recursively, checking for loops.
    std::set<base::ptr<File>> visited_files;
    check_and_add_sections(definition, file, visited_files, archive);
  }
}

void sa::Linker::check_and_add_sections(
  base::ptr<sa::Occurrence> definition,
  base::ptr<File> file,
  std::set<base::ptr<File>> &visited_files,
  File *archive
)
{
  assert(project->is_locked());
  visited_files.insert(file);
  trace_nest("check and add sections for " << file->get_name());
  trace("has linkable file kind: " << file->has_linkable_file_kind());
  trace("mode: " << file->get_mode());
  if (allow_file(file, archive)) {
    add_sections_instantiating(definition, file);
  }
  // Always check includers: includes are always honored, even includes of
  // force-excluded files.
  auto includers = file->get_includers();
  for (auto includer: includers) {
    trace("check " << *includer->file << " due to " << *includer);
    if (visited_files.find(includer->file) == visited_files.end()) {
      // Unimplemented optimisation: check that the includer actually includes
      // this file.
      check_and_add_sections(definition, includer->file, visited_files,archive);
    }
  }
}

void sa::Linker::add_sections_instantiating(
  base::ptr<sa::Occurrence> definition,
  base::ptr<File> file
)
{
  assert(project->is_locked());
  for (auto section: file->get_sections()) {
    assert(base::is_valid(section));
    if (section->instantiates_occurrence(definition)) {
      trace_nest("include " << *section << " to resolve " << *definition);
      add_section(section, definition);
    } else {
      //trace(*file << " does not instantiate " << *definition);
    }
  }
}

void sa::Linker::add_section(sa::Section *section, Occurrence *definition)
{
  assert(project->is_locked());
  assert(base::is_valid(section));
  if (linked_sections.find(section) != linked_sections.end()) {
    trace("section already included: " << *section);
    return;
  }
  trace_nest("add section " << *section);
  if (definition) {
    trace("for " << *definition);
  } else {
    trace("(forced)");
  }
  linked_sections.insert(section);
  //
  // Update status_map. The status map contains the link status for all symbols
  // involved in the current link session; other symbols are not in the map.
  //
  // Also add symbols used in the new section to the todo-list.
  //
  for (auto def:
         section->get_global_occurrences(sa::OccurrenceKind_definition)
  ) {
    trace_nest("add " << *def);
    add_definition(def);
  }
  for (auto def:
         section->get_global_occurrences(sa::OccurrenceKind_tentative_definition)
  ) {
    trace_nest("add " << *def);
    add_tentative_definition(def);
  }
  for (auto def:
         section->get_global_occurrences(sa::OccurrenceKind_weak_definition)
  ) {
    trace_nest("add " << *def);
    add_weak_definition(def);
  }
  trace("process uses:");
  trace("process required globals:");
  for (auto symbol: section->get_required_globals()) {
    require(symbol->link_name.data());
  }
  for (auto symbol: section->get_weakly_required_globals()) {
    weakly_require(symbol->link_name.data());
  }
}

void sa::Linker::add_definition(Occurrence *definition)
{
  assert(base::is_valid(definition));
  trace_nest("add definition " << *definition);
  assert(definition->kind == OccurrenceKind_definition);
  auto symbol = definition->entity->as_global_symbol();
  LinkStatus &status = status_map[symbol->link_name.data()];
  if (status < LinkStatus_defined) {
    if (status < LinkStatus_weakly_defined) {
      trace("add defined: " << *definition);
      defined.push_back(symbol->name.data());
    }
    status = LinkStatus_defined;
    
  } else {
    status = LinkStatus_multiply_defined;
    _linker_status = LinkerStatus_error;
  }
  trace("add definition for " << symbol->name << " -> " << status);
}
  
void sa::Linker::add_tentative_definition(Occurrence *definition)
{
  assert(base::is_valid(definition));
  assert(definition->kind == OccurrenceKind_tentative_definition);
  auto symbol = definition->entity->as_global_symbol();
  LinkStatus &status = status_map[symbol->link_name.data()];
  if (status < LinkStatus_tentatively_defined) {
    if (status < LinkStatus_weakly_defined) {
      trace("add defined: " << *definition);
      defined.push_back(symbol->name.data());
    }
    status = LinkStatus_tentatively_defined;
  }
  trace("add tentative definition for " << symbol->name << " -> " << status);
}
  
void sa::Linker::add_weak_definition(Occurrence *definition)
{
  assert(base::is_valid(definition));
  assert(definition->kind == OccurrenceKind_weak_definition);
  auto symbol = definition->entity->as_global_symbol();
  LinkStatus &status = status_map[symbol->link_name.data()];
  if (status < LinkStatus_weakly_defined) {
    trace("add defined: " << *definition);
    defined.push_back(symbol->name.data());
    status = LinkStatus_weakly_defined;
  }
  trace("add weak definition for " << symbol->name << " -> " << status);
}

void sa::Linker::require_section_defs(sa::Section *section)
{
  trace_nest("require defs of section " << *section);
  assert(base::is_valid(section));
  for (auto def:
         section->get_global_occurrences(sa::OccurrenceKind_definition)
  ) {
    require(def->entity->as_global_symbol()->link_name.data());
  }
  for (auto def:
         section->get_global_occurrences(sa::OccurrenceKind_tentative_definition)
  ) {
    require(def->entity->as_global_symbol()->link_name.data());
  }
  for (auto def:
         section->get_global_occurrences(sa::OccurrenceKind_weak_definition)
  ) {
    require(def->entity->as_global_symbol()->link_name.data());
  }
}
  
void sa::Linker::require(const std::string &name)
{
  // If the symbol has not been resolved yet,  add it do the todo-list.
  //
  // A symbol that has already been resolved must not be put on the todo-list
  // again. It is not only unnecessary but also wrong. Once a symbol is resolved
  // in one library or object file, gcc never attempts to resolve it again in
  // another library, and we need to mimick that behavior.
  //
  if (resolved.find(name) == resolved.end()) {
    trace("require: " << name);
    todo.push_back(name);
    resolved.insert(name);
  }
  // Symbol might have been weakly required.  Make sure it is at least strongly
  // required now.
  LinkStatus &status = status_map[name];
  if (status < LinkStatus_undefined) {
    status = LinkStatus_undefined;
  }
  trace("required " << name << ": " << status_map[name]);
}

void sa::Linker::weakly_require(const std::string &name)
{
  // If the symbol has not been resolved yet,  add it do the todo-list.
  //
  // A symbol that has already been resolved must not be put on the todo-list
  // again. It is not only unnecessary but also wrong. Once a symbol is resolved
  // in one library or object file, gcc never attempts to resolve it again in
  // another library, and we need to mimick that behavior.
  //
  if (resolved.find(name) == resolved.end()) {
    trace("weakly require: " << name);
    todo.push_back(name);
    resolved.insert(name);
    LinkStatus &status = status_map[name];
    if (status < LinkStatus_weakly_undefined) {
      status = LinkStatus_weakly_undefined;
    }
    trace("required " << name << ": " << status_map[name]);
  }
}

void sa::Linker::report_linker_status()
{
  trace("Set linker status " << _reported_linker_status << " -> "
    << _linker_status
  );
  assert_(is_locked(), _linker_status);
  if (_reported_linker_status != _linker_status) {
    _reported_linker_status = _linker_status;
    project->report_linker_status(_linker_status);
  }
}

void sa::Linker::on_out_of_date()
{
  trace_nest("Linker on_out_of_date");
  project->set_project_status(ProjectStatus_busy);
  _linker_status = LinkerStatus_waiting;
  report_linker_status();
}

void sa::Linker::on_status(Status status)
{
  trace_nest("Linker on_status " << status);
  if (status == Running) {
    _linker_status = LinkerStatus_busy;
    report_linker_status();
  }
}
