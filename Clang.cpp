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

#include "Clang.h"
#include "Analyzer.h"
#include "Unit.h"
#include "Project.h"
#include "LocalSymbol.h"
#include "base/debug.h"
#include "base/filesystem.h"
#include "base/Timer.h"
#include "base/os.h"
#include "base/print.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Mangle.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Frontend/FrontendAction.h"
#include "Embeetle.h"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <algorithm>

static base::Timer analysis_timer("Clang analysis (internal)");
static base::Timer index_timer("Index");
static base::Timer extract_timer("Extract Clang data");

// 
// C
// C Standard 	__STDC__ 	__STDC_VERSION__
// C89/C99        1           #undef
// C94            1           199409L
// C99            1           199901L
// C11            1           201112L
// C17            1           201710L
// C2X, GNU2x     1           strictly larger than 201710L
//
// C++
// C++ Standard __STDC__    __cplusplus 	__cplusplus_cli
// C++98          1           199711L         #undef
// C++11          1           201103L         #undef
// C++14          1           201402L         #undef
// C++17          1           201703L         #undef
// C++2a, GNU++2a 1           strictly larger than 201703L #undef
// C++/CLI                    #undef          200406L

namespace clang {
  class ASTUnit;
}

namespace sa {

  inline bool ends_with(std::string const &value, std::string const &ending)
  {
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
  }

  enum CompilerKind {
    CompilerKind_other,
    CompilerKind_GCC,
    CompilerKind_AVR,
  };
  
  static CompilerKind get_compiler_kind(const std::string &compiler)
  {
    const std::string &base_name = base::get_command_base_name(compiler);
    if (ends_with(base_name, "avr-gcc") || ends_with(base_name, "avr-g++") ) {
      trace("An Arduino compiler: " << compiler);
      return CompilerKind_AVR;
    }
    if (ends_with(base_name, "gcc") || ends_with(base_name, "g++") ) {
      trace("A GCC compiler: " << compiler);
      return CompilerKind_GCC;
    }
    trace("Another compiler: " << compiler);
    return CompilerKind_other;
  }
    
  static unsigned get_compiler_version(const std::string &compiler)
  {
    trace("compiler version for " << compiler);
    // TODO: extract real compiler version by running the compiler with -v ?
    return 0;
  }
    
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
    );    
  }
  
  std::ostream &operator<<(std::ostream &out,
    clang::FunctionDecl::TemplatedKind kind
  )
  {
    static const char *names[] = {
      "non-template",
      "function-template",
      "member-specialization",
      "function-template-specialization",
      "dependent-function-template-specialization",
      "dependent-non-template",
    };
    assert(kind < sizeof(names)/sizeof(names[0]));
    return out << names[kind];
  }

  std::ostream &operator<<(std::ostream &out,
    clang::TemplateSpecializationKind kind
  )
  {
    static const char *names[] = {
      // Not a specialization or instantiation
      "undeclared",

      // Implicit instantiation (full specialization)
      "implicit",

      // Explicit specialization (full or partial)
      "explicit",

      // Explicit instantiation declaration
      "explicit instantiation declaration",
      
      // Explicit instantiation definition
      "explicit instantiation definition",
    };
    assert(kind < sizeof(names)/sizeof(names[0]));
    return out << names[kind];
  }

  std::ostream &operator<<(std::ostream &out,
    clang::DiagnosticsEngine::Level level
  )
  {
    static const char *names[] = {
      "ignored",
      "note",
      "remark",
      "warning",
      "error",
      "fatal",
    };
    assert(level < sizeof(names)/sizeof(names[0]));
    return out << names[level];
  }

  std::ostream &operator<<(std::ostream &out,
    clang::VarDecl::DefinitionKind kind
  )
  {
    static const char *names[] = {
      // This declaration is only a declaration.
      "declaration",
      
      // This declaration is a tentative definition.
      "tentative definition",

      // Definition
      "definition",
    };
    assert(kind < sizeof(names)/sizeof(names[0]));
    return out << names[kind];
  }

  std::ostream &operator<<(std::ostream &out, clang::Linkage linkage)
  {
    static const char *names[] = {
      // No linkage, which means that the entity is unique and can only be
      // referred to from within its scope.
      "no",
    
      // Internal linkage, which indicates that the entity can be referred to
      // from within the translation unit (but not other translation units).
      "internal",
    
      // External linkage within a unique namespace. From the language
      // perspective, these entities have external linkage. However, since they
      // reside in an anonymous namespace, their names are unique to this
      // translation unit, which is equivalent to having internal linkage from
      // the code-generation point of view.
      "unique-external",
    
      // No linkage according to the standard, but is visible from other
      // translation units because of types defined in a inline function.
      "visible-no",
    
      // Internal linkage according to the Modules TS, but can be referred to
      // from other translation units indirectly through inline functions and
      // templates in the module interface.
      "module-internal",
    
      // Module linkage, which indicates that the entity can be referred to from
      // other translation units within the same module, and indirectly from
      // arbitrary other translation units through inline functions and
      // templates in the module interface.
      "module",
    
      // External linkage, which indicates that the entity can be
      // referred to from other translation units.
      "external",
    };
    assert(linkage < sizeof(names)/sizeof(names[0]));
    return out << names[linkage];
  }

  std::ostream &operator<<(
    std::ostream &out,
    clang::PPCallbacks::FileChangeReason reason
  )
  {
    static const char *names[] = {
      "enter file",
      "exit file",
      "system header pragma",
      "rename file",
    };
    assert(reason < sizeof(names)/sizeof(names[0]));
    return out << names[reason];
  }

  std::ostream &operator<<(std::ostream &out, const llvm::StringRef &str)
  {
    return out << str.str();
  }
  
  std::ostream &operator<<(std::ostream &out, const clang::QualType &type)
  {
    return out << type.getAsString();
  }

  std::ostream &operator<<(std::ostream &out, const clang::TypeLoc &tloc)
  {
    return out << tloc.getType().getTypePtr()->getTypeClassName()
        << " '" << tloc.getType() << "'";
  }
  
  std::ostream &operator<<(std::ostream &out, clang::Type *type)
  {
    if (type) {
      out << type->getTypeClassName() << "Type '"
          << type->getCanonicalTypeUnqualified() << "'";
    } else {
      out << "<null-type>";
    }
    return out;
  }
  
  std::ostream &operator<<(std::ostream &out, clang::Stmt *stmt)
  {
    if (stmt) {
      out << stmt->getStmtClassName();
    } else {
      out << "<null-statement>";
    }
    return out;
  }
  
  std::ostream &operator<<(std::ostream &out, const clang::Decl *decl)
  {
    if (decl) {
      out << decl->getDeclKindName() << "Decl";
      if (auto named_decl = llvm::dyn_cast<clang::NamedDecl>(decl)) {
        out << " " << named_decl->getQualifiedNameAsString();
      }
    } else {
      out << "<null-decl>";
    }
    return out;
  }

  inline void remove_template_parameters(std::string &name)
  {
    size_t pos = name.find('<');
    if (pos != std::string::npos) {
      name.resize(pos);
    }
  }

  // Get the full name for a named decl, including scopes and function signature
  // for C++.
  //
  // Clang appends formal template parameters for a templated class constructor
  // or destructor; remove those. The template name refers to a template, no
  // need to append formal parameters whose names are irrelevant.
  static std::string get_full_name(clang::NamedDecl *decl)
  {
    std::string name = decl->getQualifiedNameAsString();
    switch (decl->getDeclName().getNameKind()) {
      case clang::DeclarationName::CXXConstructorName:
      case clang::DeclarationName::CXXDestructorName:
        remove_template_parameters(name);
        break;
      default:
        break;
    }
    return name;
  }

  // Get size of name in source code,  to set source range
  static size_t get_name_size(clang::NamedDecl *decl, bool is_implicit)
  {
    if (is_implicit) {
      return 0;
    }
    std::string name = decl->getNameAsString();
    if (decl->isTemplated()) {
      size_t pos = name.find('<');
      if (pos != std::string::npos) {
        name.resize(pos);
      }
    }
    return std::max(name.size(),(size_t)1);
  }

  static std::string location_string(
    const clang::SourceLocation &loc,
    clang::SourceManager &sm
  )
  {
    std::ostringstream out;
    out << '"' << sm.getFilename(loc).str() << '"'
        << " " << sm.getSpellingLineNumber(loc)
        << " " << sm.getSpellingColumnNumber(loc)
      //<< " in-file:" << loc.isFileID()
      // << " in-macro:" << loc.isMacroID()
      ;
    return out.str();
  }

  static std::string range_string(
    const clang::SourceRange &range,
    clang::SourceManager &sm
  )
  {
    (void)range_string;
    std::ostringstream out;
    out
      << location_string(range.getBegin(),sm)
      << "..."
      << location_string(range.getEnd(),sm);
    return out.str();
  }
                  
  class ClangLocator {
    // Base class,  handling locations in Clang
    // From the documentation of SourceLocation:
    // Encodes a location in the source.
    //
    // The SourceManager can decode this to get at the full include stack, line
    // and column information.
    //
    // Technically, a source location is simply an offset into the manager's
    // view of the input source, which is all input buffers (including macro
    // expansions) concatenated in an effectively arbitrary order. The manager
    // actually maintains two blocks of input buffers. One, starting at offset 0
    // and growing upwards, contains all buffers from this module. The other,
    // starting at the highest possible offset and growing downwards, contains
    // buffers of loaded modules.
    //
    // In addition, one bit of SourceLocation is used for quick access to the
    // information whether the location is in a file or a macro expansion.
    //
    // It is important that this type remains small. It is currently 32 bits
    // wide.
    //
    // To do : figure out all location variants available in SourceManager
    //
    // Spelling location
    //
    // Expansion location
    //
  public:
    Clang *const analyzer;
    
    ClangLocator(Clang *analyzer)
      : analyzer(analyzer)
    {
    }

    clang::SourceLocation get_spelling_loc(
      const clang::SourceLocation &loc,
      clang::SourceManager &sm
    )
    {
      return sm.getSpellingLoc(loc);
    }

    clang::SourceLocation get_expansion_loc(
      const clang::SourceLocation &loc,
      clang::SourceManager &sm
    )
    {
      return sm.getExpansionLoc(loc);
    }

    bool is_in_file(
      const clang::SourceLocation &loc,
      clang::SourceManager &sm
    )
    {
      return !sm.getFilename(loc).empty();
    }

    base::ptr<File> get_file(
      const clang::SourceLocation &loc,
      clang::SourceManager &sm
    )
    {
      return analyzer->get_file(sm.getFilename(loc).str());
    }

    unsigned get_offset(
      const clang::SourceLocation &loc,
      clang::SourceManager &sm
    )
    {
      return sm.getFileOffset(loc);
    }
    
    FileLocation get_location(
      const clang::SourceLocation &loc,
      clang::SourceManager &sm
    )
    {
      base::ptr<File> file = get_file(loc, sm);
      unsigned offset = get_offset(loc, sm);
      return FileLocation(file, offset);
    }

    FileLocation get_spelling_location(
      const clang::SourceLocation &loc,
      clang::SourceManager &sm
    )
    {
      return get_location(get_spelling_loc(loc, sm), sm);
    }

    FileLocation get_expansion_location(
      const clang::SourceLocation &loc,
      clang::SourceManager &sm
    )
    {
      return get_location(get_expansion_loc(loc, sm), sm);
    }
    
    template<typename ClangRangeType> Range get_range(
      const ClangRangeType &range,
      clang::SourceManager &sm
    )
    {
      unsigned begin_offset = get_offset(range.getBegin(), sm);
      unsigned end_offset = get_offset(range.getEnd(), sm);
      assert(begin_offset <= end_offset);
      return Range(begin_offset, end_offset);
    }
  };

  class ClangAnalyzer: public ClangLocator {
  public:
    clang::SourceManager &sm;
    
    ClangAnalyzer(Clang *analyzer, clang::SourceManager &sm)
      : ClangLocator(analyzer)
      , sm(sm)
    {
    }

    bool is_in_file(const clang::SourceLocation &loc)
    {
      return ClangLocator::is_in_file(loc, sm);
    }
    
    FileLocation get_location(const clang::SourceLocation &loc)
    {
      return ClangLocator::get_location(loc, sm);
    }

    clang::SourceLocation get_spelling_loc(const clang::SourceLocation &loc)
    {
      return ClangLocator::get_spelling_loc(loc, sm);
    }

    clang::SourceLocation get_expansion_loc(const clang::SourceLocation &loc)
    {
      return ClangLocator::get_expansion_loc(loc, sm);
    }

    clang::SourceLocation get_best_loc(const clang::SourceLocation &loc)
    {
      auto best_loc = get_expansion_loc(loc);
      if (!is_in_file(best_loc)) {
        best_loc = get_spelling_loc(loc);
      }
      return best_loc;
    }

    base::ptr<File> get_file(const clang::SourceLocation &loc)
    {
      return ClangLocator::get_file(loc, sm);
    }

    unsigned get_offset(const clang::SourceLocation &loc)
    {
      return ClangLocator::get_offset(loc, sm);
    }

    FileLocation get_spelling_location(const clang::SourceLocation &loc)
    {
      return ClangLocator::get_spelling_location(loc, sm);
    }

    FileLocation get_expansion_location(const clang::SourceLocation &loc)
    {
      return ClangLocator::get_expansion_location(loc, sm);
    }

    template <class T>
    FileLocation get_spelling_location(const T *item)
    {
      return get_spelling_location(item->getLocation());
    }

    FileLocation get_spelling_location(const clang::SourceRange &range)
    {
      return get_spelling_location(range.getBegin());
    }

    Range get_range(const clang::SourceLocation &loc, unsigned size)
    {
      unsigned offset = sm.getFileOffset(loc);
      return Range(offset, offset + size);
    }

    template <typename ClangRangeType>
    Range get_range(const ClangRangeType &range)
    {
      return ClangLocator::get_range(range, sm);
    }
    
    template <class T>
    std::string location(const T *item)
    {
      return location(item->getLocation());
    }

    std::string location(const clang::SourceRange &range)
    {
      return location(range.getBegin());
    }

    std::string location(const clang::SourceLocation &loc)
    {
      return location_string(loc, sm);
    }
  };

  // Preprocessor callbacks.  See clang/Lex/PPCallbacks.h for more override
  // opportunities.
  class PreprocessorCallbacks: public ClangAnalyzer, public clang::PPCallbacks {
    bool in_real_file = false;
    struct MacroInfo {
      base::ptr<sa::Symbol> symbol;
      bool is_defined = false;
    };
    std::map<std::string, MacroInfo> macros;

  protected:
    std::string get_macro_name(const clang::Token &token)
    {
      return static_cast<std::string>(token.getIdentifierInfo()->getName());
    }

    void add_macro_occurrence(
      const clang::Token &token,
      OccurrenceKind okind,
      bool drop = false
    )
    {
      std::string name = get_macro_name(token);
      clang::SourceLocation spelling_loc = get_spelling_loc(token.getLocation());
      trace_nest("add macro " << okind << " " << name << " drop=" << drop);
      trace("spelling loc: " << location(spelling_loc)
        << " in-file=" << is_in_file(spelling_loc)
      );
      // Clang sometimes reports internal definitions (e.g. for __UINT8_TYPE__)
      // as well as internal uses (what are those???).
      // For now, we report internal definitions but not other uses.
      if (is_in_file(spelling_loc) || okind == OccurrenceKind_definition) {
        MacroInfo &info = macros[name];
        if (!info.symbol
          || (okind == OccurrenceKind_definition && info.is_defined)
        ) {
          trace("add macro symbol " << name);
          info.symbol = analyzer->get_local_symbol(
            EntityKind_macro, name, get_location(spelling_loc)
          );
        }
        analyzer->add_occurrence(
          info.symbol,
          okind,
          OccurrenceStyle_unspecified,
          analyzer->get_section(),
          get_file(spelling_loc),
          get_range(spelling_loc, name.size()),
          false
        );
        if (okind == OccurrenceKind_definition) {
          info.is_defined = true;
        }
        if (drop) {
          info.symbol = 0;
          info.is_defined = false;
        }
      }
    }
    
  public:
    PreprocessorCallbacks(
      Clang *analyzer,
      clang::SourceManager &sm
    ): ClangAnalyzer(analyzer, sm)
    {
      trace("Create preprocessor callbacks");
    }
    
    ~PreprocessorCallbacks() override
    {
      trace("Destroy preprocessor callbacks");
    }

    void FileChanged(
      clang::SourceLocation Loc,
      FileChangeReason Reason,
      clang::SrcMgr::CharacteristicKind FileType,
      clang::FileID PrevFID
    )
    {
      trace("PP file changed because " << Reason
        << " @" << location(Loc)
        << " " << PrevFID.isValid()
      );
      // This is a hack, to efficiently skip macro definitions that are
      // predefined or defined on the command line.
      if (PrevFID.isValid()) {
        in_real_file = true;
      }
    }
    
    /// Callback invoked whenever an inclusion directive of
    /// any kind (\c \#include, \c \#import, etc.) has been processed, regardless
    /// of whether the inclusion will actually result in an inclusion.
    ///
    /// \param HashLoc The location of the '#' that starts the inclusion
    /// directive.
    ///
    /// \param IncludeTok The token that indicates the kind of inclusion
    /// directive, e.g., 'include' or 'import'.
    ///
    /// \param FileName The name of the file being included, as written in the
    /// source code.
    ///
    /// \param IsAngled Whether the file name was enclosed in angle brackets;
    /// otherwise, it was enclosed in quotes.
    ///
    /// \param FilenameRange The character range of the quotes or angle brackets
    /// for the written file name.
    ///
    /// \param File The actual file that may be included by this inclusion
    /// directive, or null if the file was not found on the search path.
    ///
    /// \param SearchPath Contains the search path which was used to find the
    /// file in the file system. If the file was found via an absolute include
    /// path, SearchPath will be empty. For framework includes, the SearchPath
    /// and RelativePath will be split up. For example, if an include of
    /// "Some/Some.h" is found via the framework path
    /// "path/to/Frameworks/Some.framework/Headers/Some.h", SearchPath will be
    /// "path/to/Frameworks/Some.framework/Headers" and RelativePath will be
    /// "Some.h".
    //
    //  Note: SearchPath is always an absolute path. If a relative -I path is
    //  given on the command line, it is relative to the build directory.
    ///
    /// \param RelativePath The path relative to SearchPath, at which the include
    /// file was found. This is equal to FileName except for framework includes.
    ///
    /// \param Imported The module, whenever an inclusion directive was
    /// automatically turned into a module import or null otherwise.
    ///
    /// \param FileType The characteristic kind, indicates whether a file or
    /// directory holds normal user code, system code, or system code which is
    /// implicitly 'extern "C"' in C++ mode.
    ///
    //
    // If the #include statement uses quotes (not angled backets) and the hdir
    // path is the directory of the including file, then no hdir was involved.
    //
    void InclusionDirective(
      clang::SourceLocation hash_loc,
      const clang::Token &include_token,
      clang::StringRef filename,  // as written in source code
      bool is_angled,
      clang::CharSourceRange filename_range,
      const clang::OptionalFileEntryRef file, // null if not found
      clang::StringRef search_path, // only valid when file is found
      clang::StringRef relative_path, // as appended to search path; can be abs
      const clang::Module *imported_module,
      clang::SrcMgr::CharacteristicKind file_type
    ) override
    {
      trace("#include directive " << is_angled << "@" << location(hash_loc)
        << " of " << filename << " search-path='" << search_path
        << "' rel-path='" << relative_path
        << "' " << location(filename_range.getBegin())
        << " .. " << location(filename_range.getEnd())
        << " file=" << (bool)file
      );
      if (!file) {
        analyzer->report_missing_header(static_cast<std::string>(filename));
      } else {
        //
        // If #include <...> was not found on search path but exists locally,
        // Clang issues an error like:
        //
        //   'foo.h' file not found with <angled> include; use "quotes" instead
        //
        // and continues with the local header.  Here, we need to report a
        // missing header, to re-trigger analysis when the file is added.
        //
        if (is_angled && search_path.empty()) {
          analyzer->report_missing_header(static_cast<std::string>(filename));
        }
        //
        // Get SA file pointers and source range.
        llvm::StringRef includer_path =
          sm.getFilename(filename_range.getBegin());
        base::ptr<File> includer = analyzer->get_file(
          static_cast<std::string>(includer_path)
        );
        Range range = get_range(filename_range);
        range.end = range.begin + filename.size() + 2; // Do we need this?
        //
        // Clang does not tell us whether the includee was found locally (=in
        // the includers directory) or via an hdir.  We want to know this,
        // because we want to determine whether each hdir is used or not.
        //
        // The simplest way is by checking whether the search path is equal to
        // the includer's path up to the last slash. No need to test this for
        // angled #include's.
        //
        // Does this #include use an hdir?
        //
        bool has_hdir = is_angled
          || search_path != includer_path.take_front(includer_path.rfind('/'));
        std::string hdir = has_hdir ? static_cast<std::string>(search_path) : "";
        //
        // Check for non-portable file names.  Curent check is not complete, but
        // catches the most common issues.
        size_t non_portable_char_index = relative_path.find_first_of(":\\*?\"<>%");
        if (non_portable_char_index != llvm::StringRef::npos) {
          analyzer->report_diagnostic(
            ("non-portable character '"
              + relative_path.substr(non_portable_char_index,1)
              + "' in included file path '" + relative_path + "'"
            ).str(),
            Severity_error, includer, range.begin, false
          );
        }
        //
        // On Windows, if the relative path has a different case on disk then in
        // the source code, use the case on disk. Otherwise, there will be
        // distinct files in the SA that are folded to a single entry in the
        // filetree, which confuses the file status updates (red and green
        // icons).
        //
        // Optionally generate an error: such code will work but is not
        // portable. However, it seems that Clang already generates a warning,
        // which can be turned into an error using
        // -Werror=nonportable-include-path and
        // -Werror=nonportable-system-include-path, or alternatively -Wall.
        //
        std::string path_on_disk;
        if (
          base::os::path_has_different_case_on_disk(
            relative_path, path_on_disk, search_path
          )
        ) {
          analyzer->report_diagnostic(
            ("non-portable path '" + relative_path
              + "' differs in case from path on disk '" + path_on_disk + "'"
            ).str(),
            Severity_error, includer, range.begin, false
          );
        }
        // Note: analyzer->get_file will make sure to return a file with a path
        // patched to the case on disk, so no need to patch it here as well.
        std::string includee_path = static_cast<std::string>(file->getName());
        base::ptr<File> includee = analyzer->get_file(includee_path);
        //
        // Report an absolute path as not portable.  Note: a filename that looks
        // absolute may still be included as a relative path, so check the
        // search path.
        if (search_path.empty() && base::is_absolute_path(relative_path)) {
          analyzer->report_diagnostic(
            ("non-portable absolute path to file '" + relative_path + "'").str(),
            Severity_warning, includer, range.begin, false
          );
        }
        trace("#include directive @" << location(hash_loc)
          << " of " << filename << " " << relative_path
          << "\n includer=" << includer->get_name()
          << "\n includee=" << includee->get_name()
          << "\n hdir=    " << hdir
        );
        analyzer->add_include(includee, includer, range, hdir);
      }
    }

    /// Called by Preprocessor::HandleMacroExpandedIdentifier when a
    /// macro invocation is found.
    void MacroExpands(
      const clang::Token &MacroNameTok,
      const clang::MacroDefinition &MD,
      clang::SourceRange Range,
      const clang::MacroArgs *Args
    ) override
    {
      add_macro_occurrence(MacroNameTok, OccurrenceKind_use);
    }

    /// Hook called whenever a macro definition is seen.
    void MacroDefined(
      const clang::Token &MacroNameTok,
      const clang::MacroDirective *MD
    ) override
    {
      if (in_real_file) {
        add_macro_occurrence(MacroNameTok, OccurrenceKind_definition);
      }
    }

    /// Hook called whenever a macro \#undef is seen.
    /// \param MacroNameTok The active Token
    /// \param MD A MacroDefinition for the named macro.
    /// \param Undef New MacroDirective if the macro was defined, null otherwise.
    ///
    /// MD is released immediately following this callback.
    void MacroUndefined(
      const clang::Token &MacroNameTok,
      const clang::MacroDefinition &MD,
      const clang::MacroDirective *Undef
    ) override
    {
      if (in_real_file) {
        add_macro_occurrence(MacroNameTok, OccurrenceKind_use, true);
      }
    }

    /// Hook called whenever the 'defined' operator is seen.
    /// \param MD The MacroDirective if the name was a macro, null otherwise.
    void Defined(
      const clang::Token &MacroNameTok,
      const clang::MacroDefinition &MD,
      clang::SourceRange Range
    ) override
    {
      add_macro_occurrence(MacroNameTok, OccurrenceKind_use);
    }

    /// Hook called when a source range is skipped.
    /// \param Range The SourceRange that was skipped. The range begins at the
    /// \#if/\#else directive and ends after the \#endif/\#else directive.
    /// \param EndifLoc The end location of the 'endif' token, which may precede
    /// the range skipped by the directive (e.g excluding comments after an
    /// 'endif').
    void SourceRangeSkipped(
      clang::SourceRange Range,
      clang::SourceLocation EndifLoc
    ) override
    {
    }

    /// Hook called whenever an \#if is seen.
    /// \param Loc the source location of the directive.
    /// \param ConditionRange The SourceRange of the expression being tested.
    /// \param ConditionValue The evaluated value of the condition.
    ///
    // FIXME: better to pass in a list (or tree!) of Tokens.
    void If(
      clang::SourceLocation Loc,
      clang::SourceRange ConditionRange,
      ConditionValueKind ConditionValue
    ) override
    {
    }

    /// Hook called whenever an \#elif is seen.
    /// \param Loc the source location of the directive.
    /// \param ConditionRange The SourceRange of the expression being tested.
    /// \param ConditionValue The evaluated value of the condition.
    /// \param IfLoc the source location of the \#if/\#ifdef/\#ifndef directive.
    // FIXME: better to pass in a list (or tree!) of Tokens.
    void Elif(
      clang::SourceLocation Loc,
      clang::SourceRange ConditionRange,
      ConditionValueKind ConditionValue,
      clang::SourceLocation IfLoc
    ) override
    {
    }

    /// Hook called whenever an \#ifdef is seen.
    /// \param Loc the source location of the directive.
    /// \param MacroNameTok Information on the token being tested.
    /// \param MD The MacroDefinition if the name was a macro, null otherwise.
    void Ifdef(
      clang::SourceLocation Loc,
      const clang::Token &MacroNameTok,
      const clang::MacroDefinition &MD
    ) override
    {
      add_macro_occurrence(MacroNameTok, OccurrenceKind_use);
    }

    /// Hook called whenever an \#elifdef branch is taken.
    /// \param Loc the source location of the directive.
    /// \param MacroNameTok Information on the token being tested.
    /// \param MD The MacroDefinition if the name was a macro, null otherwise.
    void Elifdef(
      clang::SourceLocation Loc,
      const clang::Token &MacroNameTok,
      const clang::MacroDefinition &MD
    ) override
    {
      add_macro_occurrence(MacroNameTok, OccurrenceKind_use);
    }
    
    /// Hook called whenever an \#elifdef is skipped.
    /// \param Loc the source location of the directive.
    /// \param ConditionRange The SourceRange of the expression being tested.
    /// \param IfLoc the source location of the \#if/\#ifdef/\#ifndef directive.
    // FIXME: better to pass in a list (or tree!) of Tokens.
    void Elifdef(
      clang::SourceLocation Loc,
      clang::SourceRange ConditionRange,
      clang::SourceLocation IfLoc
    ) override
    {
    }

    /// Hook called whenever an \#ifndef is seen.
    /// \param Loc the source location of the directive.
    /// \param MacroNameTok Information on the token being tested.
    /// \param MD The MacroDefiniton if the name was a macro, null otherwise.
    void Ifndef(
      clang::SourceLocation Loc,
      const clang::Token &MacroNameTok,
      const clang::MacroDefinition &MD
    ) override
    {
      add_macro_occurrence(MacroNameTok, OccurrenceKind_use);
    }

    /// Hook called whenever an \#elifndef branch is taken.
    /// \param Loc the source location of the directive.
    /// \param MacroNameTok Information on the token being tested.
    /// \param MD The MacroDefinition if the name was a macro, null otherwise.
    void Elifndef(
      clang::SourceLocation Loc,
      const clang::Token &MacroNameTok,
      const clang::MacroDefinition &MD
    ) override
    {
      add_macro_occurrence(MacroNameTok, OccurrenceKind_use);
    }
    
    /// Hook called whenever an \#elifndef is skipped.
    /// \param Loc the source location of the directive.
    /// \param ConditionRange The SourceRange of the expression being tested.
    /// \param IfLoc the source location of the \#if/\#ifdef/\#ifndef directive.
    // FIXME: better to pass in a list (or tree!) of Tokens.
    void Elifndef(
      clang::SourceLocation Loc,
      clang::SourceRange ConditionRange,
      clang::SourceLocation IfLoc
    ) override
    {
    }

    /// Hook called whenever an \#else is seen.
    /// \param Loc the source location of the directive.
    /// \param IfLoc the source location of the \#if/\#ifdef/\#ifndef directive.
    void Else(
      clang::SourceLocation Loc,
      clang::SourceLocation IfLoc
    ) override
    {
    }

    /// Hook called whenever an \#endif is seen.
    /// \param Loc the source location of the directive.
    /// \param IfLoc the source location of the \#if/\#ifdef/\#ifndef directive.
    void Endif(
      clang::SourceLocation Loc,
      clang::SourceLocation IfLoc
    ) override
    {
    }

    // Embeetle-specific hook called whenever an invalid UTF-8 character is
    // found.
    void InvalidUTF8(
      clang::SourceLocation Loc
    ) override
    {
      trace("invalid UTF-8 at " << location(Loc));
      analyzer->report_non_utf8_file(get_file(Loc));
    }
  };

  class ClangVisitor
    : public ClangAnalyzer
    , public clang::RecursiveASTVisitor<ClangVisitor>
  {
    // The recursive AST visitor is a bit unusual.  It has three methods for
    // each class Foo in the AST:
    //
    //  1. TraverseFoo does the traversal. It is only defined for some classes:
    //     the most basic base classes, leaf classes and classes with children
    //     in the AST. This means you cannot use it to change the traversal by
    //     overriding it for an arbitrary class. Nodes are visited in top-down
    //     order.
    //
    //      a. call WalkUpFromFoo
    //      b. visit children (nested nodes) of Foo
    //
    //     The main base classes for which TraverseFoo is defined are:
    //
    //      - Decl: base class for all declarations and definitions
    //
    //      - Stmt: base class for all statements and expressions,
    //        most notably DeclRefExpr which represents a reference to a Decl
    //
    //      - Type: base class for all types,  both declared and built-in;
    //        declared types have both a derived Decl and a derive Type class
    //
    //      - TypeLoc: base class for all type references; role is similar to
    //        that of DeclRefExpr, except that there is a different subclass for
    //        different types
    //
    //     For more, search the internet for 'clang RecursiveASTVisitor'
    //
    //  2. WalkUpFromFoo is defined for all classes. Default implementation:
    //
    //      a. WalkUpFrom<base class of Foo>
    //      b. VisitFoo
    //
    //  3. VisitFoo is defined for all classes,  and does nothing by default.
    //
    // To examine the methods of RecursiveASTVisitor, check
    // llvm/clang/include/clang/AST/RecursiveASTVisitor.h
    //
    // A large part of that file is generated using the preprocessor. The
    // simplest way to examine at - e.g. to find the correct method signatures
    // for overriding - is to build the preprocessed Clang.i and examine that.
    //
    // The calls to TraverseFoo are not virtual (so you should not use the
    // 'override' keyword).  Instead, the Clang code casts the visitor to the
    // class passed in as template parameter, and then makes a direct call.  The
    // same may be true for WalkUpFromFoo and VisitFoo; to be checked.  This
    // allows strong compiler optimization for empty VisitFoo functions.
    //
    // Since TraverseFoo cannot be overridden for an arbitrary class, it is not
    // possible to use it to open a scope before any class and close it scope
    // afterwards.  Instead, we use an is_open flag that is set when a scope is
    // opened in an overridden WalkUpFromFoo or VisitFoo class. When the flag is
    // set, the scope will be closed automatically by TraverseDecl or
    // TraverseType, which are the most basic Traverse methods. This way, the
    // WalkUpFromFoo or VisitFoo method can simply call open_scope() and forget
    // about calling close_scope().
    //
    // All these methods return a bool.  Returning false aborts the traversal.
    //
    typedef clang::RecursiveASTVisitor<ClangVisitor> Base;
    
    clang::ASTContext &ast;
    clang::Preprocessor &preprocessor;
    clang::ASTNameGenerator mangler;
    clang::LangStandard lang_standard;
    char optimization_level;
    CompilerKind compiler_kind;
    unsigned compiler_version;

    std::map<clang::Decl*, base::ptr<Symbol>> _symbol_map;
    std::set<void*> _generated_methods;

    // True iff the current decl opened a scope.
    bool scope_is_open = false;
    friend class Scope;

  protected:

    // Open scope for the just emitted decl.  It will be closed automatically in
    // a Traverse*(...) method after traversal.
    void open_scope()
    {
      trace("open scope");
      assert(!scope_is_open);
      scope_is_open = true;
      analyzer->open_scope();
    }

    // Auxiliary class to automatically close an opened scope in basic
    // TraverseDecl and TraverseType methods. This saves the is_open flag when
    // created and restores it when destroyed.
    class Scope {
      ClangVisitor *visitor;
      bool is_open;
    public:
      Scope(ClangVisitor *visitor)
        : visitor(visitor)
        , is_open(visitor->scope_is_open)
      {
        visitor->scope_is_open = false;
      }

      ~Scope()
      {
        if (visitor->scope_is_open) {
          trace("close scope");
          visitor->analyzer->close_scope();
        }
        visitor->scope_is_open = is_open;
      }
    };

    // Local entity processing.

    // Uses of global symbols in local entities only require the global symbol
    // to be defined if the local entity is used directly or indirectly from at
    // least one global symbol.

    static bool is_local_entity_kind(EntityKind kind)
    {
      return kind == EntityKind_local_function
        || kind == sa::EntityKind_automatic_variable
        || kind == sa::EntityKind_static_variable
        || kind == sa::EntityKind_local_static_variable
        ;
    }
    
    struct LocalEntity {
      std::vector<LocalEntity*> local_uses;
      std::vector<Occurrence*> global_uses;

      void add_to_section(
        Analyzer *analyzer, Section *section, std::set<LocalEntity*> &done
      )
      {
        if (!done.count(this)) {
          for (auto use: global_uses) {
            analyzer->add_global_occurrence_to_section(use, section);
          }
          done.insert(this);
          for (auto function: local_uses) {
            function->add_to_section(analyzer, section, done);
          }
        }
      }
    };
    
    std::map<LocalSymbol*, LocalEntity> local_entities;
    
    std::map<Section*, std::vector<LocalEntity*>> local_uses;

    void register_occurrence(Occurrence *occurrence, Section *section)
    {
      trace_nest("register occurrence " << *occurrence
        << " is_global_symbol=" << occurrence->entity->is_global_symbol()
        << " top-scope=" << analyzer->top_scope()
        << " cur-scope=" << analyzer->current_scope()
      );
      if (occurrence->kind == OccurrenceKind_use) {
        Occurrence *scope = analyzer->top_scope();
        if (scope) {
          trace("register occurrence " << *occurrence << " in " << *scope);
          if (scope->entity->is_global_symbol()) {
            if (is_local_entity_kind(occurrence->entity->kind)) {
              LocalEntity &entity = local_entities[
                occurrence->entity->as_local_symbol()
              ];
              local_uses[section].push_back(&entity);
              trace("register use of local entity in global entity");
              return;
            }
          } else if (is_local_entity_kind(scope->entity->kind)) {
            LocalSymbol *local_entity = scope->entity->as_local_symbol();
            if (occurrence->entity->is_global_symbol()) {
              local_entities[local_entity].global_uses.push_back(occurrence);
              trace("postpone use of global symbol in local entity "
                << *scope
              );
              return;
            } else if (is_local_entity_kind(occurrence->entity->kind)) {
              LocalEntity &callee = local_entities[
                occurrence->entity->as_local_symbol()
              ];
              local_entities[local_entity].local_uses.push_back(&callee);
              trace("register use of local entity in local entity");
              return;
            }
          }
        }
      }
      if (occurrence->entity->is_global_symbol()) {
        analyzer->add_global_occurrence_to_section(occurrence, section);
      }
    }

    void finalize_locals()
    {
      for (auto const &[section, used_local_entities]: local_uses) {
        std::set<LocalEntity*> done;
        for (auto entity: used_local_entities) {
          entity->add_to_section(analyzer, section, done);
        }
      }
    }
      
  public:
    explicit ClangVisitor(Clang *analyzer, clang::ASTUnit *ast_unit
      , char optimization_level
      , CompilerKind compiler_kind
      , unsigned compiler_version
    )
      : ClangAnalyzer(analyzer, ast_unit->getASTContext().getSourceManager())
      , ast(ast_unit->getASTContext())
      , preprocessor(ast_unit->getPreprocessor())
      , mangler(ast)
      , lang_standard(
        clang::LangStandard::getLangStandardForKind(
          ast_unit->getLangOpts().LangStd
        )
      )
      , optimization_level(optimization_level)
      , compiler_kind(compiler_kind)
      , compiler_version(compiler_version)
    {
    }

    ~ClangVisitor()
    {
      // Clear the symbol map here, with the project locked, as this might
      // delete symbols.
      Project::Lock lock(analyzer->project);
      _symbol_map.clear();
    }

    void traverse()
    {
      trace_nest("traverse AST");
      TraverseAST(ast);
      finalize_locals();
    }

    bool TraverseDecl(clang::Decl *decl)
    {
      if (!decl) return true;
      if (decl->isImplicit()) return true;

      trace_nest("Traverse decl " << decl << " " << location(decl));
      Scope scope(this);
      Base::TraverseDecl(decl);
      return true;
    }

    bool TraverseStmt(clang::Stmt *stmt, DataRecursionQueue *queue = nullptr)
    {
      //if (stmt && llvm::isa<clang::CompoundStmt>(stmt)) {
      trace_nest("Traverse stmt " << stmt);
      if (auto expr = llvm::dyn_cast_or_null<clang::DeclRefExpr>(stmt)) {
        trace("  `-> " << expr->getDecl());
      }
      return Base::TraverseStmt(stmt, queue);
    }
    
    bool TraverseType(clang::QualType type)
    {
      trace_nest("Traverse type " << type);
      Scope scope(this);
      return Base::TraverseType(type);
    }

    bool TraverseTypeLoc(clang::TypeLoc tloc)
    {
      trace_nest("traverse type loc " << tloc);
      return Base::TraverseTypeLoc(tloc);
    }

    bool WalkUpFromWhileStmt(clang::WhileStmt *stmt)
    {
      trace_nest("walk up from " << stmt);
      report_loop_if_empty(stmt, stmt->getBody());
      return Base::WalkUpFromWhileStmt(stmt);
    }

    bool WalkUpFromDoStmt(clang::DoStmt *stmt)
    {
      trace_nest("walk up from " << stmt);
      report_loop_if_empty(stmt, stmt->getBody());
      return Base::WalkUpFromDoStmt(stmt);
    }

    bool WalkUpFromForStmt(clang::ForStmt *stmt)
    {
      trace_nest("walk up from " << stmt);
      report_loop_if_empty(stmt, stmt->getBody());
      return Base::WalkUpFromForStmt(stmt);
    }

    bool WalkUpFromCXXForRangeStmt(clang::CXXForRangeStmt *stmt)
    {
      trace_nest("walk up from " << stmt);
      report_loop_if_empty(stmt, stmt->getBody());
      return Base::WalkUpFromCXXForRangeStmt(stmt);
    }

    void report_loop_if_empty(clang::Stmt *loop, clang::Stmt *body)
    {
      if (auto block = llvm::dyn_cast<clang::CompoundStmt>(body)) {
        if (block->body_empty()) {
          report_empty_loop(loop);
        }
      } else if (llvm::dyn_cast<clang::NullStmt>(body)) {
        report_empty_loop(loop);
      }
    }
      
    void report_empty_loop(clang::Stmt *loop)
    {
      trace("Empty loop: " << loop);
      clang::SourceLocation begin_loc = get_spelling_loc(loop->getBeginLoc());
      clang::SourceLocation end_loc = get_spelling_loc(loop->getEndLoc());
      trace(location(begin_loc));
      trace(location(end_loc));
      if (is_in_file(begin_loc)) {
        base::ptr<File> file = get_file(begin_loc);
        if (get_file(end_loc) == file) {
          auto begin_offset = get_offset(begin_loc);
          auto end_offset = get_offset(end_loc);
          assert(end_offset > begin_offset);
          analyzer->add_empty_loop(file, Range(begin_offset, end_offset+1));
        }
      }
    }
      
    bool WalkUpFromNamedDecl(clang::NamedDecl *decl)
    {
      emit(
        decl, decl->getLocation(), get_entity_kind(decl),
        OccurrenceKind_definition
      );
      return true;
    }

    //--------------------------------------------------------------------------
    // Template declarations and specializations
    //--------------------------------------------------------------------------
    
    bool WalkUpFromTemplateDecl(clang::TemplateDecl *decl)
    {
      trace_nest("walk up from template decl " << decl);
      return true;
    }

    bool WalkUpFromClassTemplateDecl(clang::ClassTemplateDecl *decl)
    {
      trace_nest("walk up from class template decl " << decl);
      emit(decl, decl->getLocation(), get_entity_kind(decl),
        OccurrenceKind_definition
      );
      open_scope();
      return true;
    }

    bool WalkUpFromClassTemplateSpecializationDecl(
      clang::ClassTemplateSpecializationDecl *decl
    )
    {
      trace_nest("walk up from class template specialization decl "
        << decl
      );
      // Also represents explicit instantiations.
      clang::ClassTemplateDecl *tdecl = decl->getSpecializedTemplate();
      trace("specialized template: " << tdecl);
      OccurrenceKind okind = decl->isThisDeclarationADefinition()
        ? OccurrenceKind_definition : OccurrenceKind_declaration;
      emit(tdecl, decl->getLocation(), get_entity_kind(tdecl), okind);
      open_scope();
      return true;
    }

#if 0
    bool WalkUpFromClassTemplatePartialSpecializationDecl(
      clang::ClassTemplatePartialSpecializationDecl *decl
    )
    {
      trace_nest("walk up from class template partial specialization decl "
        << decl
      );
      return Base::WalkUpFromClassTemplatePartialSpecializationDecl(decl);
    }
#endif

    bool WalkUpFromFunctionTemplateDecl(clang::FunctionTemplateDecl *decl)
    {
      trace_nest("walk up from function template decl " << decl);
      // For an explanation of how Clang uses function template declarations,
      // see emit_function(...).

#if 0
      trace("  `--> function " << decl->getTemplatedDecl()
        << " " << decl->getTemplatedDecl()->isTemplated()
        << " " << decl->getTemplatedDecl()->isDependentContext()
        << " " << decl->getTemplatedDecl()->getTemplateSpecializationKind()
        << " " << decl->getTemplatedDecl()->getDescribedFunctionTemplate()
        << " " << decl->getTemplatedDecl()->getPrimaryTemplate()
        << " " << decl->getTemplatedDecl()->getCanonicalDecl()
        << " " << decl->getTemplatedDecl()->getInstantiatedFromDecl()
        << " " << decl->getTemplatedDecl()->getQualifiedNameAsString()
      );
      for (auto i = decl->spec_begin(); i != decl->spec_end(); ++i) {
        clang::FunctionDecl *spec = *i;
        trace("  `--> instantiation " << spec
          << " " << spec->isTemplated()
          << " " << spec->isDependentContext()
          << " " << spec->getTemplateSpecializationKind()
          << " " << spec->getDescribedFunctionTemplate()
          << " " << spec->getPrimaryTemplate()
          << " " << spec->getCanonicalDecl()
          << " " << spec->getInstantiatedFromDecl()
        );
      }
#endif
      emit(decl, decl->getLocation(), get_entity_kind(decl),
        OccurrenceKind_definition
      );
      open_scope();
      return true;
    }

#if 0
    // Does not exist!
    bool WalkUpFromFunctionTemplateSpecializationDecl(
      clang::FunctionTemplateSpecializationDecl *decl
    )
    {
      trace_nest("walk up from function template specialization decl " << decl);
      // Also represents explicit instantiations.
      clang::FunctionTemplateDecl *tdecl = decl->getSpecializedTemplate();
      trace("specialized template: " << tdecl);
      OccurrenceKind okind = decl->isThisDeclarationADefinition()
        ? OccurrenceKind_definition : OccurrenceKind_declaration;
      emit(tdecl, decl->getLocation(), get_entity_kind(tdecl), okind);
      open_scope();
      return true;
    }
#endif

    bool WalkUpFromVarTemplateDecl(clang::VarTemplateDecl *decl)
    {
      trace_nest("walk up from var template decl " << decl);
      emit(decl, decl->getLocation(), get_entity_kind(decl),
        OccurrenceKind_definition
      );
      open_scope();
      return true;
    }

    bool WalkUpFromVarTemplateSpecializationDecl(
      clang::VarTemplateSpecializationDecl *decl
    )
    {
      trace_nest("walk up from var template specialization decl " << decl);
      // Also represents explicit instantiations.
      clang::VarTemplateDecl *tdecl = decl->getSpecializedTemplate();
      trace("specialized template: " << tdecl);
      OccurrenceKind okind = decl->isThisDeclarationADefinition()
        ? OccurrenceKind_definition : OccurrenceKind_declaration;
      emit(tdecl, decl->getLocation(), get_entity_kind(tdecl), okind);
      open_scope();
      return true;
    }

    bool WalkUpFromTypeAliasTemplateDecl(clang::TypeAliasTemplateDecl *decl)
    {
      trace_nest("walk up from type alias template decl " << decl);
      return Base::WalkUpFromTypeAliasTemplateDecl(decl);
    }

    bool WalkUpFromTemplateTypeParmDecl(clang::TemplateTypeParmDecl *decl)
    {
      trace_nest("walk up from template type parm decl " << decl);
      emit(decl, decl->getLocation(), get_entity_kind(decl),
        OccurrenceKind_definition
      );
      return true;
    }

    bool WalkUpFromNonTypeTemplateParmDecl(clang::NonTypeTemplateParmDecl *decl)
    {
      trace_nest("walk up from non type template parm decl " << decl);
      emit(decl, decl->getLocation(), get_entity_kind(decl),
        OccurrenceKind_definition
      );
      return true;
    }

    bool WalkUpFromTemplateTemplateParmDecl(
      clang::TemplateTemplateParmDecl *decl
    )
    {
      trace_nest("walk up from templete template parm decl " << decl);
      return Base::WalkUpFromTemplateTemplateParmDecl(decl);
    }

    bool WalkUpFromFunctionDecl(clang::FunctionDecl *decl)
    {
      trace_nest("walk up from function decl " << (void*)decl << " " << decl
        << " with " << decl->getLinkageInternal() << " linkage, is a "
        << (decl->isTemplated() ? "templated " : "")
        << (decl->isThisDeclarationADefinition() ? "definition" : "declaration")
        << " mangled-name: " << mangler.getName(decl)
        << " is-defined: " << decl->isDefined()
        << " has-body: " << decl->doesThisDeclarationHaveABody()
        << " has-skipped-body: " << decl->hasSkippedBody()
        << " is-weak: " << decl->isWeak()
        << " has-weak-attr: " << decl->hasAttr<clang::WeakAttr>()
        << " templated-kind: "
        << decl->getTemplatedKind()
        << " template-specialization-kind: "
        << decl->getTemplateSpecializationKind()
        << " is-inlined: " << decl->isInlined()
        << " is-externally-visible: " << decl->isExternallyVisible()
        << " is-inline-definition-externally-visible: "
        << decl->isInlineDefinitionExternallyVisible()
        << " is-C++: " << lang_standard.isCPlusPlus()
        // A dependent context depends on a template parameter. 
        << " is-dependent-context: " << decl->isDependentContext()
        << " is-templated: " << decl->isTemplated()
        << " is-template-instantiation: " << decl->isTemplateInstantiation()
        << " is-function-template-specialization: "
        << decl->isFunctionTemplateSpecialization()
        << " described-function-template: "
        << decl->getDescribedFunctionTemplate()
        << " primary-template: " << decl->getPrimaryTemplate()
        << " @" << location(decl->getLocation())
        << " definition: " << (void*)decl->getDefinition()
        << " " << decl->getDefinition()
        << " is-global: " << decl->isGlobal()
        << " force-ext-visible: "
        << decl->doesDeclarationForceExternallyVisibleDefinition()
        << " most-recent: " << (void*)(decl->getMostRecentDecl())
        << " " << mangler.getName(decl->getMostRecentDecl())
        << " @" << location(decl->getMostRecentDecl()->getLocation())
        << " inlined=" << decl->getMostRecentDecl()->isInlined()
        << " is-templated: " << decl->getMostRecentDecl()->isTemplated()
        << " canonical: " << (void*)(decl->getCanonicalDecl())
        << " " << mangler.getName(decl->getCanonicalDecl())
        << " @" << location(decl->getCanonicalDecl()->getLocation())
        << " inlined=" << decl->getCanonicalDecl()->isInlined()
        << " is-templated: " << decl->getCanonicalDecl()->isTemplated()
        << " most-recent->canonical: "
        << (void*)(decl->getMostRecentDecl()->getCanonicalDecl())
        << " instantiated-from: " << decl->getInstantiatedFromDecl()
        << " " << (decl->getInstantiatedFromDecl()
          ? mangler.getName(decl->getInstantiatedFromDecl()) : "")
      );
      for (auto alt = decl->getMostRecentDecl(); alt;
           alt = alt->getPreviousDecl()
      ) {
        trace("alt decl: " << (void*)alt << " " << alt->isExternallyVisible()
          << " " << alt << " " << get_entity_kind(alt) << " "
          << mangler.getName(alt)
        );
      }
      if (decl->isInlined() && decl->isThisDeclarationADefinition()) {
        trace("is-inline-externally-visible: "
          << decl->isInlineDefinitionExternallyVisible()
        );
      }
      EntityKind mrekind = get_entity_kind(decl->getMostRecentDecl());
      trace("most-recent entity kind: " << mrekind);
      
      // Functions with a function template are not processed, because a
      // definition has already been emitted from the function template. This
      // approach is required because we want to nest the template parameters in
      // the variable template.
      trace_code(
        if (decl->getDescribedFunctionTemplate()) {
          trace("skipped - already emitted function template");
          return true;
        }
      );

      EntityKind ekind = get_entity_kind(decl);
      trace("entity kind: " << ekind);

      OccurrenceKind okind = decl->isThisDeclarationADefinition()
        && (lang_standard.isCPlusPlus()
          || !decl->getMostRecentDecl()->isInlined()
          || decl->isInlineDefinitionExternallyVisible()
        )
        ? OccurrenceKind_definition : OccurrenceKind_declaration;
      //
      // If this is a declaration (and not a definition) of an inline function,
      // and the current optimization level is greater than zero, we could also
      // instantiate a definition of a local function here, to be used to handle
      // calls that can be inlined. If all calls can be inlined, there will be
      // no use of a global symbol, so code without a definition would work.
      //
      // Such code is fragile, as it would fail to link at optimization level
      // zero.  However, I believe that some sample projects from vendors rely
      // on it.  Was this for Arduino? To be verified, the sample projects might
      // be fixed by now.
      //
      if (is_global_symbol_kind(ekind)) {
        // Detect weak definitions and declarations,  change okind accordingly.
        //
        // Weak definitions and references are an ELF feature.  Other targets do
        // not support it (except a.out format with gcc).
        //
        // A definition is weak if it has the "weak" attribute, or if a
        // declaration in the same compilation unit has the "weak" attribute.
        // C++ non-inline class methods can implicitly result in a weak
        // definition, even without the "weak" attribute.
        //
        // A weak definition is replaced by a strong definition if present at
        // link time.  Otherwise, an arbitrary weak definition is used, and
        // multiple weak definitions do not cause linktime errors. Weak
        // definitions are still limited to one per compilation unit by the
        // compiler.
        //
        // A weak definition or declaration also causes references lexically
        // after the definition or declaration to be weak.  A weak reference
        // does not cause a linktime error if the symbol is not defined.
        // Instead, the linker replaces the address of the symbol by zero. Users
        // of the symbol can test at runtime whether the symbol is defined by
        // testing for non-zero.
        //
        // References lexically before a weak declaration are still strong.
        // Note that gcc seems to have a bug, or at least unspecified behavior:
        // any test for non-zero lexically before a weak declaration succeeds,
        // possibly causing the program to crash at runtime. Try this:
        //
        //    void foo();
        //    int main() {
        //        if (&foo) {
        //            foo();
        //        }
        //    }
        //    void foo() __attribute__((weak));
        //
        // Clang handles this situation correctly, by emitting a linktime error
        // "undefined reference to `foo'"
        //
        // A strong variable definition: int var = 0;
        //
        // A tentative (common) variable definition: int var;
        //
        // A weak variable definition: int var __attribute__((weak));
        //
        // A weak variable reference: extern int var __attribute__((weak));
        //
        // A weakref attribute is a different beast; it only applies to static
        // symbols (i.e. compilation unit local symbols) and must be accompanied
        // by an alias, either within the attribute itself or as a separate
        // attribute.
        //
        // A weak_import attribute seems to be LLVM- and Apple specific.  I have
        // never seen it used in embedded code.
        //

        // A definition is weak if any declaration in the same compilation unit
        // has the "weak" attribute.  A declaration is weak if it has the "weak"
        // attribute on itself.
        //
        // Trouble is that later declarations seem to inherit attributes from
        // earlier declarations in Clang.  For example, in this code:
        //
        //     void foo();
        //     void foo() __attribute__((weak));
        //     void foo();
        //
        // the first declaration does not have the "weak" attribute, but the
        // second and third declarations do have it, although the first and
        // third declaration are textually identical. It is not clear how to
        // check for the presence on a declaration itself.
        //
        // In practice, this hasn't been a problem. We mostly want to avoid that
        // the first declaration - for example in a header file - is treated
        // differently depending on the presence or absence of something like
        // the second declaration - for example in a C file. If this proves to
        // be a problem later, we will have to revise our strategy.
        
        // Template instantiations must be treated like weak definitions, so
        // that the linker throws away all definitions except for one.  This is
        // true for both variables and functions. However, note that function
        // templates are emitted when their FunctionTemplateDecl is processed,
        // so must be skipped here.
        //
        // Implicit instantiations are generated at the first call site and will
        // be handled there. Explicit instantiations are handled here.
        //
        // An example of a templated function definition is a method in a
        // template class.  The definition of that method - either inline or
        // outside the class definition - can be in a header file without
        // causing multiply-defined global errors.
        //
        // Most inline definitions are no longer global, so the test for inlined
        // functions in headers below may seem unnecessary, but inline virtual
        // functions are still global, so for them, the test is essential.
        //
        // Also, an inline virtual function remains a virtual function, so is
        // required as soon as the class is instantiated. Unless optimization
        // level 1 or greater is used, and the compiler can determine that the
        // virtual method is never used, such as in this case:
        //
        //   extern int tip_data;
        //
        //   class Foo {
        //   public:
        //     virtual int foo() { return tip_data; }
        //   };
        //
        //   int main() {
        //     Foo foo;
        //   }
        //
        // Since the address of Foo::foo is not passed to a location invisible
        // to the compiler, and Foo::foo is not called locally, compiling this
        // code with -O1 or higher does not require a definition for tip_data.
        // Reproducing such optimization-dependent subtleties here is almost
        // impossible, and we will not attempt to do so unless we find a case
        // where it is essential.
        //
        clang::NamedDecl *decl_for_weak_attribute =
          okind == OccurrenceKind_definition ? decl->getMostRecentDecl() : decl;
        if (decl_for_weak_attribute->hasAttr<clang::WeakAttr>()
          || (lang_standard.isCPlusPlus() && (
              decl->isTemplated() || decl->getMostRecentDecl()->isInlined())
          )
        ) {
          if (okind == OccurrenceKind_definition) {
            okind = OccurrenceKind_weak_definition;
          } else {
            okind = OccurrenceKind_weak_declaration;
          }
        }
      }
      trace("function decl: " << ekind << " " << okind << " " << decl
        << " " << mangler.getName(decl)
      );
      assert_(sa::is_declaration(okind), okind);
      emit_function(decl, decl->getLocation(), ekind, okind);
      open_scope();
      return true;
    }

    bool WalkUpFromVarDecl(clang::VarDecl *decl)
    {
      trace_nest("walk up from var decl " << decl
        << " with " << decl->getLinkageInternal() << " linkage, is a "
        << (decl->isTemplated() ? "templated " : "")
        << decl->isThisDeclarationADefinition()
        << " is-static-data-member=" << decl->isStaticDataMember()
        << " has-definition=" << decl->hasDefinition()
        << " acting-definition=" << decl->getActingDefinition()
        << " definition=" << decl->getDefinition()
        << " canonical-decl=" << decl->getCanonicalDecl()
        << " has-constant-initialization=" << decl->hasConstantInitialization()
        << " is-demoted-definition="
        << decl->isThisDeclarationADemotedDefinition()
        << " get-init: " << decl->getInit()
        << " @" << location(decl->getLocation())
      );
      // Variables with a variable template are not processed, because a
      // definition has already been emitted from the variable template. This
      // approach is required because we want to nest the template parameters in
      // the variable template.
      if (decl->getDescribedVarTemplate()) {
        trace("skipped - already emitted variable template");
        return true;
      }
      // Note: formal linkage is a subset of internal linkage; internal linkage
      // is more detailed.
      OccurrenceKind okind;
      EntityKind ekind = get_entity_kind(decl);
      trace("ekind = " << ekind);
      switch (ekind) {
        case EntityKind_automatic_variable:
        case EntityKind_local_static_variable:
        case EntityKind_parameter:
          okind = OccurrenceKind_definition;
          break;
          
        case EntityKind_global_variable:
        case EntityKind_global_variable_template:
          switch (decl->isThisDeclarationADefinition()) {
            case clang::VarDecl::DeclarationOnly:
              // This case includes static const data members that are
              // initialized in their class definition:
              //
              //   class TwoWire {
              //   public:
              //     static const uint32_t TWI_CLOCK = 100000;
              // };
              //
              // According to current C++ rules such a variable is a
              // declaration, not a definition, but can be used as an r-value
              // without definition; each use will effectively be replaced by
              // the constant initial value. For details, see
              // https://www.stroustrup.com/bs_faq2.html#in-class and
              // https://stackoverflow.com/questions/3025997
              //
              // Whether a use is treated as an r-value may in practice depend
              // on compiler implementation and on optimization level, so it is
              // tricky to predict when such a symbol will treated as undefined
              // by the linker.
              //
              // As an approximate alternative, we treat a static const data
              // member with constant initialization as a weak definition.  This
              // usually has the same effect, except that in case of an l-value
              // reference, a missing definition is not detected.
              //
              // To fix this, we need a reliable way to distinguish l-values
              // from r-values, which we currently don't have. We would also
              // have to introduce a new kind of occurrence, one that provides a
              // definition for r-value uses and a declation for l-value uses.
              // Hint to get started: try clang::Expr::isLValue().  Test cases:
              // cpp.test, ar.test.
              //
              if (decl->isStaticDataMember()
                && decl->hasConstantInitialization()
              ) {
                okind = OccurrenceKind_weak_definition;
              } else if (decl->hasAttr<clang::WeakAttr>()) {
                okind = OccurrenceKind_weak_declaration;
              } else {
                okind = OccurrenceKind_declaration;
              }
              trace("set declaration only, okind=" << okind);
              break;
            case clang::VarDecl::TentativeDefinition:
              // A tentative definition is something like "int foo;" at file
              // scope.  No "external", no initialization.  Also no
              // __attribute__((weak)) anywhere in the compilation unit.
              //
              // This is equivalent to a "common" definition in assembly.
              // Strong definitions replace tentative definitions.  All
              // tentative definitions are merged into a single definition.  Do
              // weak tentative declarations exist?
              okind = OccurrenceKind_tentative_definition;
              trace("set tentative definition, okind=" << okind);
              break;
            case clang::VarDecl::Definition:
              // A definition is weak if any declaration in the same
              // compilation unit has the "weak" attribute.
              //
              // Template instantiations must treated like weak definitions,
              // so that the linker throws away all definitions except for
              // one.  This is true for both variables and functions.
              //
              // An example of a templated variable definition is a static
              // variable in a template class.  The definition of that
              // static variable - either inline or outside the class
              // definition - can be in a header file without causing
              // multiply-defined global errors.
              //
              // From the C++ language standard, section 3.2 p5:
              //
              //    There can be more than one definition of ... static data
              //    member of a class template ... in a program provided
              //    that each definition appears in a different translation
              //    unit,
              //
              // and
              //
              //     If the definitions of D satisfy all these requirements,
              //     then the program shall behave as if there were a single
              //     definition of D.
              //
              // Having a local definition in each compilation unit will not
              // do, because the definitions will have different addresses
              // and values.  This requirement is often not met when there
              // are local definitions in a dll; the dll definitions
              // typically remain distinct.
              //
              // Clang treats template instantiation as a kind of template
              // specialization.
              //
              if (decl->getMostRecentDecl()->isWeak()
                || decl->getTemplateSpecializationKind()
                != clang::TSK_Undeclared
              ) {
                okind = OccurrenceKind_weak_definition;
              } else {
                okind = OccurrenceKind_definition;
              }
              trace("set ordinary definition, okind=" << okind);
              break;
            default:
              assert(false);
              okind = OccurrenceKind_definition;
          }
          break;
              
        case EntityKind_static_variable:
          switch (decl->isThisDeclarationADefinition()) {
            case clang::VarDecl::DeclarationOnly:
              if (decl->isWeak()) {
                okind = OccurrenceKind_weak_declaration;
              } else {
                okind = OccurrenceKind_declaration;
              }
              break;
            case clang::VarDecl::TentativeDefinition:
              // A tentative definition is something like "int foo;" at file
              // scope.  No "external", and also no initialization.  This is
              // equivalent to a "common" definition in assembly.  Strong
              // definitions replace tentative definitions.  All tentative
              // definitions are merged into a single definition.  Do weak
              // tentative declarations exist?
              okind = OccurrenceKind_tentative_definition;
              break;
            case clang::VarDecl::Definition:
              if (decl->isWeak()) {
                okind = OccurrenceKind_weak_definition;
              } else {
                okind = OccurrenceKind_definition;
              }
              break;

            default:
              assert(false);
              okind = OccurrenceKind_definition;
          }
          break;

        default:
          trace("not implemented yet");
          return true;
      }
      emit_variable(decl, decl->getLocation(), ekind, okind);
      if (is_definition(okind) && definition_can_be_scope(ekind)
        && decl->hasInit()
      ) {
        open_scope();
      }
      return true;
    }

    bool WalkUpFromTypeDecl(clang::TypeDecl *decl)
    {
      trace_nest("walk up from type decl " << decl);
      emit(
        decl, decl->getLocation(), EntityKind_type, OccurrenceKind_definition
      );
      return true;
    }

    bool WalkUpFromEnumDecl(clang::EnumDecl *decl)
    {
      trace_nest("walk up from enum decl " << decl);
      sa::OccurrenceKind okind = get_tag_okind(decl);
      emit(decl, decl->getLocation(), EntityKind_enum, okind);
      if (sa::is_definition(okind)) {
        open_scope();
      }
      return true;
    }

    // TODO: if a TagDecl is anonymous, scope it under the surrounding
    // declaration, which can be a typedef, a variable or a field.  The problem
    // is that WalkUpFromTagDecl is called *before* the surrounding typedef,
    // variable or field is visited, so that the current Analyzer interface does
    // not allow us to nest it.

    bool WalkUpFromCXXRecordDecl(clang::CXXRecordDecl *decl)
    {
      trace_nest("walk up from C++ record decl " << decl);
      if (decl->hasDefinition()) {
        // Following queries are only allowed when the class has a definition;
        // otherwise, a Clang assertion will fail.
        trace("needs implicit default constructor: " <<
          decl->needsImplicitDefaultConstructor()
        );
        trace("needs implicit copy constructor: " <<
          decl->needsImplicitCopyConstructor()
        );
        trace("needs implicit move constructor: " <<
          decl->needsImplicitMoveConstructor()
        );
        trace("needs implicit copy assignment: " <<
          decl->needsImplicitCopyAssignment()
        );
        trace("needs implicit destructor: " <<
          decl->needsImplicitDestructor()
        );
      }
      
      // For a class template, Clang generates and visits both a ClassTemplate
      // and a CXXRecordDecl.  The CXXRecordDecl lists the members, while the
      // ClassTemplate lists the template parameters.  Every instantiation of
      // the template will generate yet another CXXRecordDecl.
      //
      // We only emit the ClassTemplate; instantiations will be emitted where
      // they are instantiated.
      if (decl->getDescribedClassTemplate()) {
        trace("skipped - already emitted class template");
        return true;
      }
      return Base::WalkUpFromCXXRecordDecl(decl);
    }

    bool WalkUpFromRecordDecl(clang::RecordDecl *decl)
    {
      trace_nest("walk up from record decl " << decl);
      EntityKind ekind = get_entity_kind(decl);
      OccurrenceKind okind = get_tag_okind(decl);
      emit(decl, decl->getLocation(), ekind, okind);
      if (sa::is_definition(okind)) {
        open_scope();
      }
      return true;
    }

    bool WalkUpFromFieldDecl(clang::FieldDecl *decl)
    {
      trace_nest("walk up from field decl " << decl);
      emit(
        decl, decl->getLocation(), EntityKind_field, OccurrenceKind_definition
      );
      return true;
    }

    // A using declaration to import overloaded method declarations from a base
    // class.  This is useful when overriding one of the declarations in the
    // derived class.  Without using declaration, C++ will hide the other
    // declaration in the derived class.
    bool WalkUpFromBaseUsingDecl(clang::BaseUsingDecl *decl)
    {
      trace_nest("walk up from base using decl " << decl);
      for (auto i = decl->shadow_begin(); i != decl->shadow_end(); ++i) {
        clang::UsingShadowDecl *shadow = *i;
        clang::NamedDecl *target = shadow->getTargetDecl();
        trace("shadow target " << target);
        emit(
          target, decl->getLocation(), get_entity_kind(target),
          OccurrenceKind_declaration
        );
      }
      return true;
    }

    // Using-shadow declarations are handled in WalkUoFromBaseUsingDecl above,
    // so should not be handled again. AFAIK, WalkUpFromUsingShadowDecl is not
    // called directly when traversing the AST. Just in case, I provide an empty
    // implementation here anyway.
    bool WalkUpFromUsingShadowDecl(clang::UsingShadowDecl *decl)
    {
      trace_nest("walk up from using shadow decl " << decl);
      return true;
    }

    // Using-pack declarations: what exactly are they? Ignore for now. TODO.
    bool WalkUpFromUsingPackDecl(clang::UsingPackDecl *decl)
    {
      trace_nest("walk up from using pack decl " << decl);
      emit(decl, decl->getLocation(), get_entity_kind(decl), OccurrenceKind_use);
      return true;
    }

    // Using declaration that was annotated with attribute((using_if_exists))
    // failed to resolve to a known declaration. We don't handle that situation
    // here. Skip the default behavior.
    bool WalkUpFromUnresolvedUsingIfExistsDecl(
      clang::UnresolvedUsingIfExistsDecl *decl
    )
    {
      return true;
    }

    // A using directive declaration is not a definition but a use.
    bool WalkUpFromUsingDirectiveDecl(
      clang::UsingDirectiveDecl *decl
    )
    {
      emit(
        decl, decl->getIdentLocation(), get_entity_kind(decl),
        OccurrenceKind_use
      );
      return true;
    }

    bool VisitDecl(clang::Decl *decl)
    {
      trace("unhandled decl " << decl << " " << location(decl->getLocation()));
      return true;
    }
    
    // Entity kinds:

    EntityKind get_entity_kind(clang::NamedDecl *decl)
    {
      if (auto function_decl = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
        return get_entity_kind(function_decl);
      }
      if (auto var_decl = llvm::dyn_cast<clang::VarDecl>(decl)) {
        return get_entity_kind(var_decl);
      }
      if (auto type_decl = llvm::dyn_cast<clang::TypeDecl>(decl)) {
        return get_entity_kind(type_decl);
      }
      if (llvm::isa<clang::FieldDecl>(decl)) {
        return EntityKind_field;
      }
      if (llvm::isa<clang::EnumConstantDecl>(decl)) {
        return EntityKind_enum_constant;
      }
      if (auto tdecl = llvm::dyn_cast<clang::TemplateDecl>(decl)) {
        return get_entity_kind(tdecl);
      }
      if (auto pdecl = llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(decl)) {
        return get_entity_kind(pdecl);
      }
      return EntityKind_other;
    }

    EntityKind get_entity_kind(clang::TypeDecl *decl)
    {
      if (llvm::isa<clang::EnumDecl>(decl)) {
        return EntityKind_enum;
      }
      if (auto record_decl = llvm::dyn_cast<clang::RecordDecl>(decl)) {
        return get_entity_kind(record_decl);
      }
      if (auto parm_decl = llvm::dyn_cast<clang::TemplateTypeParmDecl>(decl)) {
        return get_entity_kind(parm_decl);
      }
      return EntityKind_type;
    }

    EntityKind get_entity_kind(clang::RecordDecl *decl)
    {
      if (decl->isUnion()) {
        // We currently don't distinguish between C++ unions and C unions.
        return EntityKind_union;
      } else {
        if (auto cdecl = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
          return get_entity_kind(cdecl);
        } else {
          return EntityKind_struct;
        }
      }
    }

    EntityKind get_entity_kind(clang::CXXRecordDecl *decl)
    {
      return EntityKind_class;
    }

    EntityKind get_entity_kind(clang::TemplateDecl *decl)
    {
      if (auto tdecl = llvm::dyn_cast<clang::ClassTemplateDecl>(decl)) {
        return get_entity_kind(tdecl);
      }
      if (auto tdecl = llvm::dyn_cast<clang::FunctionTemplateDecl>(decl)) {
        return get_entity_kind(tdecl);
      }
      if (auto tdecl = llvm::dyn_cast<clang::VarTemplateDecl>(decl)) {
        return get_entity_kind(tdecl);
      }
      return EntityKind_other;
    }

    EntityKind get_entity_kind(clang::ClassTemplateDecl *decl)
    {
      return EntityKind_class_template;
    }

    EntityKind get_entity_kind(clang::FunctionTemplateDecl *decl)
    {
      return EntityKind_global_function_template;
    }

    EntityKind get_entity_kind(clang::VarTemplateDecl *decl)
    {
      return EntityKind_global_variable_template;
    }

    EntityKind get_entity_kind(clang::TemplateTypeParmDecl *decl)
    {
      return EntityKind_template_parameter;
    }

    EntityKind get_entity_kind(clang::NonTypeTemplateParmDecl *decl)
    {
      return EntityKind_template_parameter;
    }

    EntityKind get_entity_kind(clang::FunctionDecl *decl)
    {
      // Usually, decl and decl->getMostRecentDecl() have the same entity kind,
      // but not always. During development for HPMicro, a case has been found
      // where this matters. Simplified code has been added to
      // templ.test,  and even simpler code is in fa.test.
      //
      //   template<class T> class A {
      //     void foo() {
      //       void bar();  // (1)
      //       bar();
      //     }
      //   };
      //  
      //   void bar() {} // (2)
      //   // could also be 'void bar();' if bar is defined elsewhere
      //  
      //   template<class T> class B {
      //     void foo() {
      //       void bar(); // (3)
      //       bar();
      //     }
      //   };
      //
      // In all cases, 'bar' refers to a global (external) function, but that is
      // not how Clang interprets it. The explanation below is largely a guess
      // based on observations; I did not actually check the Clang code yet.
      //
      // Occurrence (1) is in a templated context, so Clang sees it as a
      // function template (is-templated returns true, no mangled name, equal to
      // its own canonical decl and most-recent decl,
      // templated-kind=non-template, is-dependent-context=1).
      //
      // Occurrence (2) is handled as a global function: is-templated returns
      // false, has a mangled name _Z3barv, equal to its own canonical decl and
      // (3) as most recent decl. From now on, Clang knows that `bar` is a
      // global function.
      //
      // Occurrence (3) is again in a templated context, so Clang sees it as a
      // function template: is-templated returns true, no mangled name,
      // templated-kind=non-template, is-dependent-context=1. However, Clang
      // now knows that this is a global function, and the canonical decl is
      // set to (2). Most recent decl is set to (3).
      //
      // If class A is instantiated, Clang now realises its mistake in handling
      // (1). It doesn't correct (1), but creates a new FunctionDecl (4) at the
      // same location as (1). Since (4) is created last, it is also the
      // most-recent-decl for (2) and (3). However, (4) is not visited, and (1)
      // is. (2), (3) and (4) all have the same canonical decl (2).
      //
      // Here is another example, also extracted from an HPMicro project::
      //
      //   extern "C" {
      //     template<typename T> void foo(T t) {
      //       void __failed_assertion();   // (5)
      //       __failed_assertion();
      //     }
      //     void __failed_assertion();     // (6)
      //   }
      //
      // (5) is templated and has no mangled name. (6) is global and has a
      // mangled name. Canonical decl for both is (5), and most-recent decl for
      // both is (6).
      //
      // In the first example, using the canonical decl to derive the entity
      // kind helps to correctly interpret bar (2) and (3) as global, but (1)
      // remains templated. In the second example, using the canonical decl
      // makes both (5) and (6) templated.
      //
      // Using the most-recent decl reverses this: (5) and (6) are now correctly
      // global, but (1) (2) and (3) aren't.
      //
      // If foo is called, Clang will again insert an extra declaration (7) at
      // the location of (5), and make it a global function.
      //
      // Whichever decl we use to determine the entity kind, it is essential
      // that we use the same one to determine the mangled name.  Otherwise, we
      // end up with an empty mangled name for a global entity, which causes an
      // assertion to fail.
      //
      // To determine whether a function is inlined, it suffices to check the
      // most-recent decl; see further. However, as explained above, this does
      // not reliably allow us to determine if a function is templated, unless
      // the function is instantiated/called.
      //
      // For now, we use the most-recent decl, as it gives the most reliable
      // results. It will at least generate the use of a global when the global
      // is used. We still need to find a way to handle decl's in a templated
      // context before the first non-templated context. Such decls seem
      // completely disconnected from their later replacements.
      //
      // Global, local and inline functions
      // ----------------------------------
      //
      // For linking, the SA distinguishes two kinds of functions: global or
      // local. Global functions are handled by the linker, and local functions
      // are handled within a compilation unit.
      //
      // Compilers have a lot of freedom for how to handle local functions: they
      // can be inlined, or emitted as static functions, or even emitted as weak
      // global functions.
      //
      // Inline functions are tricky: see
      // https://stackoverflow.com/questions/216510/what-does-extern-inline-do
      //
      // For all observed cases, local functions are only emitted when
      // the function is also called locally. This is true regardless of the
      // optimization level.
      //
      // Also for all observed cases, no inlining is performed with -O0, and
      // inlining is usually performed at higher optimization levels.  This is
      // true for all tested language variants: C++, C99, GNU99, GNU89. C89 does
      // not support inlining.
      //
      // A function With __attribute__((always_inline)) is always inlined when
      // possible, which is not the case when its address is taken. This
      // attribute is not relevant for our processing here.
      //
      // When a local function is processed here in Clang.cpp, we do not
      // necessarily know whether it will be called or not; calls can come
      // before or after the definition. So, when we emit global uses in the
      // function body, we must not immediately mark these uses as
      // linker-relevant; they will only be linker-relevant when the function is
      // called (directly or indirectly). To handle this, we create a temporary
      // object representing the local function and register all accesses to
      // global and local functions in its body. We also keep track of all calls
      // to local functions from global functions.  Then, in a post-processing
      // pass, we process each local call in a global function by recursively
      // traversing the local function and adding all global uses to the global
      // function.
      //
      // C++ emits inline functions as weak global functions when it decides not
      // to inline them. This is a good strategy to avoid code duplication for
      // inline functions declared in header files. Since the decision to emit a
      // weak definition depends on compiler optimization flags and details of
      // the code, other compilation units cannot rely on such a definition
      // being generated, so for SA purposes, such a definition can be treated
      // like other local functions.
      //
      //  0. C++ static functions are either emitted as static functions or
      //     inlined. The difference is irrelevant for our purposes. The
      //     "extern" keyword is optional and has no effect.
      //
      //     * is C++: yes
      //     * is inlined: yes
      //     * is inline definition externally visible: yes
      //     * external linkage: yes
      //     * is externally visible: yes
      //
      // In C, there are three different ways to declare an inline function.
      //
      //  1. C99/GNU99 "inline" is the same as GNU89 "extern inline".
      //     Out-of-line code is never emitted. If the function is not inlined,
      //     a use of a global symbol is emitted.  This happens with -O0, when
      //     the address of the function is taken, and when the compiler decides
      //     that the function is too complex for useful inlining.
      //
      //     We handle this as a declaration of a global function. At -O0, all
      //     uses are uses of a global symbol with the name of the inline
      //     function. What to do at higher optimization levels? Should we try
      //     to guess what the compiler does? There may be sample projects
      //     relying on this behavior ...
      //
      //     * is C++: no
      //     * is inlined: yes
      //     * is inline definition externally visible: no
      //     * external linkage: yes
      //     * is externally visible: yes
      //
      //     In C99/GNU99, to force emission of a definition (global & strong)
      //     after "inline void flop() {...}", use "void flop();" *after* the
      //     inline definition in exactly one compilation unit. This effectively
      //     transfers the function to case 2 below.
      //
      //  2. C99/GNU99 "extern inline" is the same as GNU89 "inline". A global
      //     definition is always emitted.  Calls may also be inlined. We handle
      //     this as a global definition and ignore the "inline" keyword.
      //
      //     * is C++: no
      //     * is inlined: yes
      //     * is inline definition externally visible: yes
      //     * external linkage: yes
      //     * is externally visible: yes
      //
      //  3. In all C variants, "static inline" may emit a static definition
      //     and/or inline the function.  We handle this as a definition of a
      //     local function.
      //
      //     * is C++: no
      //     * is inlined: yes
      //     * is inline definition externally visible: no
      //     * external linkage: no
      //     * is externally visible: no
      //
      //     C never accepts two inline definitions of the same function in
      //     the same compilation unit; not even if one is extern and the other
      //     is not.
      //
      // A global function definition is included in the unit's section, or a
      // separate section when --function-sections is present. All uses of
      // global symbols result in a required definition for the linker.
      //
      // A local function definition is only included in sections containing a
      // use of that function. Any uses of global symbols result in a required
      // definition in the section containing the use. Any uses of inline
      // functions in the inline function are applied recursively.
      //
      // Specifically, an inline function using an undefined global symbol
      // should *not* result in a linker error if the function is not
      // called. See inline.test, inline_cpp.test, inline_c99.test and
      // inline_c89.test.
      //

      if (lang_standard.isCPlusPlus()) {
        if (decl->isExternallyVisible()) {
          // From Clang source code comments for isTemplated(): a
          // declaration is templated if it is a template or a template
          // pattern, or is within (lexically for a friend or local function
          // declaration, semantically otherwise) a dependent context.
          //
          // In other words, a templated declaration is any declaration that can
          // depend on template parameters that need to be resolved at compile
          // time, so is not an actual object in the binary code.
          
          trace("is externally visible");
          if (decl->getMostRecentDecl()->isTemplated()) {
            return EntityKind_global_function_template;
          }
          if (auto method_decl = llvm::dyn_cast<clang::CXXMethodDecl>(decl)) {
            return get_entity_kind(method_decl);
          }
          // Whether the function is inlined must be derived from the most recent
          // declaration.  For example:
          //
          //   void foo();           // (1)
          //   ...                   // code potentially calling foo
          //   inline void foo() { } // (2)
          //   void foo();           // (3)
          //
          // (1) is a global function declaration
          // (2) is a local (inline) function definition
          // (3) is a local (inline) function declaration
          //
          // Instances (1), (2) and (3) all have (1) as canonical declaration
          // and (3) as most recent declaration.
          //
          // All instances refer to the same local inline function foo.  Clang
          // derives that (3) is inline from definition (2), but doesn't do that
          // for (1), which results in conflicting entity kinds for foo unless
          // we check the most recent declaration.
          //
          if (decl->getMostRecentDecl()->isInlined()) {
            return EntityKind_local_function;
          }
          return EntityKind_global_function;
        }
      } else {
        if (decl->isExternallyVisible()) {
          return EntityKind_global_function;
        }
      }
      return EntityKind_local_function;
    }

    EntityKind get_entity_kind(clang::CXXMethodDecl *decl)
    {
      return decl->isVirtual() ? EntityKind_virtual_function
        : decl->getMostRecentDecl()->isInlined() ? EntityKind_local_function
        : EntityKind_global_function;
    }

    EntityKind get_entity_kind(clang::CXXConstructorDecl *decl)
    {
      return decl->getMostRecentDecl()->isInlined() ? EntityKind_local_function
        : EntityKind_global_function;
    }

    EntityKind get_entity_kind(clang::VarDecl *decl)
    {
      trace_nest("get VarDecl entity kind " << decl);
      trace("formal linkage: " << decl->getFormalLinkage());
      trace("internal linkage: " << decl->getLinkageInternal());

      switch (decl->getFormalLinkage()) {
        case clang::NoLinkage:
          if (llvm::isa<clang::ParmVarDecl>(decl)) {
            return EntityKind_parameter;
          } else if (decl->hasLocalStorage()) {
            return EntityKind_automatic_variable;
          } else {
            return EntityKind_local_static_variable;
          }
          
        case clang::InternalLinkage:
        case clang::UniqueExternalLinkage:
          // Unique external linkage is external linkage within an anonymous
          // namespace; formally external, but equivalent to internal for
          // all practical purposes. Do we want to make a distinction?
          return EntityKind_static_variable;
                        
        case clang::VisibleNoLinkage:
        case clang::ModuleLinkage:
        case clang::ModuleInternalLinkage:
        case clang::ExternalLinkage:
          // VisibleNoLinkage and ModuleLinkage can expose symbols to other
          // translation units, so we will handle them as external linkage.  To
          // be checked: is that correct/acceptable?
          //
          // From Clang source code comments for isTemplated(): a
          // declaration is templated if it is a template or a template
          // pattern, or is within (lexcially for a friend or local function
          // declaration, semantically otherwise) a dependent context.
          //
          // In other words, a templated declaration is any declaration that
          // can depend on template parameters that need to be resolved at
          // compile time, so is not an actual object in the binary code.
          if (decl->getMostRecentDecl()->isTemplated()) {
            return EntityKind_global_variable_template;
          } else {
            return EntityKind_global_variable;
          }
      }
      assert(false);
      return EntityKind_other;
    }

    // Expressions:
    bool WalkUpFromDeclRefExpr(clang::DeclRefExpr *expr)
    {
      clang::ValueDecl *decl = expr->getDecl();
      trace_nest("handle decl ref expr " << expr << " to " << decl);
      EntityKind ekind = get_entity_kind(decl);
      trace("ekind: " << ekind);
      OccurrenceKind okind = decl->isWeak()
        ? OccurrenceKind_weak_use : OccurrenceKind_use;
      emit_any_decl(decl, expr->getLocation(), ekind, okind);
      return true;
    }

    bool WalkUpFromCXXConstructExpr(clang::CXXConstructExpr *expr)
    {
      trace_nest("handle C++ constructor expr " << expr);
      clang::CXXConstructorDecl *decl = expr->getConstructor();
      trace("locs: " << location(expr->getBeginLoc())
        << "..." << location(expr->getEndLoc())
      );
      trace("implicit: " << (expr->getBeginLoc() == expr->getEndLoc()));
      bool is_implicit = expr->getBeginLoc() == expr->getEndLoc();
      trace("is implicit: " << is_implicit);
      EntityKind ekind = get_entity_kind(decl);
      emit_function(
        decl, expr->getBeginLoc(), ekind, OccurrenceKind_use, is_implicit
      );
      require_class(decl->getParent());
      generate_method_if_needed(decl, ekind);
      return true;
    }

    bool TraverseCXXTemporaryObjectExpr(
      clang::CXXTemporaryObjectExpr *expr, DataRecursionQueue *queue = nullptr
    )
    {
      trace_nest("handle C++ temporary object expression " << expr);
      // The default implementation of this method will visit the typename in a
      // temporary object constructor (the 'Foo' in an expression 'Foo(...)')
      // twice: once as a constructor call, and once as a type reference. We
      // want to create an occurrence for the constructor call only, so we need
      // to override it here.
      return WalkUpFromCXXConstructExpr(expr);
    }

    bool WalkUpFromMemberExpr(clang::MemberExpr *expr)
    {
      trace_nest("handle member expr " << expr);
      auto decl = expr->getMemberDecl();
      emit_any_decl(
        decl, expr->getMemberLoc(), get_entity_kind(decl), OccurrenceKind_use
      );
      return true;
    }

    // Type locs:
    
    bool WalkUpFromTagTypeLoc(clang::TagTypeLoc tloc)
    {
      trace_nest("handle tag type loc " << tloc
        << " @" << location(tloc.getBeginLoc())
        << " " << mangler.getName(tloc.getDecl())
      );
      auto decl = tloc.getDecl();
      emit(decl, tloc.getBeginLoc(), get_entity_kind(decl), OccurrenceKind_use);
      return true;
    }

    bool WalkUpFromTypedefTypeLoc(clang::TypedefTypeLoc tloc)
    {
      trace_nest("handle typedef type loc " << tloc 
        << " @" << location(tloc.getBeginLoc())
      );
      auto decl = tloc.getTypedefNameDecl();
      emit(decl, tloc.getBeginLoc(), get_entity_kind(decl), OccurrenceKind_use);
      return true;
    }

    bool WalkUpFromTemplateSpecializationTypeLoc(
      clang::TemplateSpecializationTypeLoc tloc
    )
    {
      trace_nest("handle template specialization type loc '" << tloc << "' @"
        << location(tloc.getBeginLoc())
      );
      if (auto decl = tloc.getTypePtr()->getAsCXXRecordDecl()) {
        trace("decl: " << decl);
        // The type used here is a template type specialization.  There are two
        // ways to obtain the corresponding ClassTemplateDecl, and neither of
        // them works in all circumstances, so we combine them.
        //
        // For testing,  try including type_traits,  or run cpp.test.
        clang::ClassTemplateDecl *tdecl = decl->getDescribedClassTemplate();
        if (!tdecl) {
          tdecl = llvm::cast<clang::ClassTemplateSpecializationDecl>(decl)
            ->getSpecializedTemplate();
        }
        emit(tdecl, tloc.getBeginLoc(), get_entity_kind(tdecl),
          OccurrenceKind_use
        );
      } else {
        trace("unimplemented");
      }
      return true;
    }

    bool WalkUpFromTemplateTypeParmTypeLoc(
      clang::TemplateTypeParmTypeLoc tloc
    )
    {
      trace_nest("handle template type parm type loc '" << tloc << "' @"
        << location(tloc.getBeginLoc())
      );
      clang::TemplateTypeParmDecl *tdecl = tloc.getDecl();
      emit(tdecl, tloc.getBeginLoc(), get_entity_kind(tdecl),
        OccurrenceKind_use
      );
      return true;
    }

    bool VisitTypeLoc(clang::TypeLoc tloc)
    {
      // A type loc is a use of a type. This includes built-in types such as
      // 'int', 'void', 'int*', 'void (*)()', etc. The generic TypeLoc visitor
      // cannot do anything, as there is no method to obtain the ValueDecl
      // declaring the used type.  Visitors for specific derived classes emit
      // type occurrences.
      trace("skip type loc '" << tloc << "' @" << location(tloc.getBeginLoc()));
      return true;
    }

    OccurrenceKind get_tag_okind(clang::TagDecl *decl)
    {
      return decl->isCompleteDefinition() ?
        OccurrenceKind_definition : OccurrenceKind_declaration;
    }

    // Generate implicitly defined method such as a compiler generated default,
    // copy or move constructor or assigment, to avoid an undefined global. No
    // need to check if this method can actually be compiler generated: if not,
    // an error has already been issued.
    void generate_method_if_needed(
      clang::CXXMethodDecl *decl,
      EntityKind ekind_if_new
    )
    {
      if (!decl->isUserProvided()) {
        auto result = _generated_methods.insert(decl);
        if (result.second) {
          trace("Generate method " << decl);
          emit(
            get_symbol(decl, ekind_if_new),
            decl->getParent()->getLocation(),
            0,
            ekind_if_new,
            OccurrenceKind_weak_definition
          );
        }
      }
    }

    void emit_any_decl(
      clang::NamedDecl *decl
      , clang::SourceLocation loc
      , EntityKind ekind_if_new
      , OccurrenceKind okind
    )
    {
      // Functions and variables can be template instantiations,  which need
      // special treatment.
      if (auto fdecl = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
        emit_function(fdecl, loc, ekind_if_new, okind);
      } else if (auto vdecl = llvm::dyn_cast<clang::VarDecl>(decl)) {
        emit_variable(vdecl, loc, ekind_if_new, okind);
      } else {
        emit(decl, loc, ekind_if_new, okind);
      }
    }

    void emit_function(
      clang::FunctionDecl *decl
      , clang::SourceLocation loc
      , EntityKind ekind_if_new
      , OccurrenceKind okind
      , bool is_implicit = false // e.g. implicit call of default constructor
    )
    {
      // This is a function or a method, which is a special case of a
      // function. A function might be instantiated from a function template,
      // and a method might be instantiated from a method template or from a
      // method of a class template.
      //
      // Template instantiations need special treatment, because the
      // instantiated function is a global function, but the function template
      // does not constitute a global definition.
      //
      // Most compilers will handle implicit instantiations by generating a
      // weak definition for the first call in the current compilation unit.
      //
      // Clang has three kinds of objects to represent a function template and
      // its instantiations:
      //
      //  1. FunctionTemplateDecl describes template parameters and associated
      //     data.  This declaration is passed directly to emit(...), not to
      //     emit_function(...), to generate the function template definition.
      //
      //     Use tdecl->getTemplatedDecl() to get the main FunctionDecl.  To
      //     iterate function instantiations, use:
      //
      //     for (auto i = tdecl->spec_begin(); i != tdecl->spec_end(); ++i) {
      //        clang::FunctionDecl *spec = *i;
      //        ...
      //     }
      //
      //     There is normally no need to iterate over instantiations; each
      //     instantiation will be visited separately.
      //
      //  2. Main FunctionDecl. This represents the function parameters and
      //     body.  Template specialization kind is "undeclared" for these
      //     FunctionDecl's, just like for non-template FunctionDecl's. This
      //     declaration is never passed to emit(...) or emit_function(...).
      //
      //     Use getDescribedFunctionTemplate() to get the function template.
      //
      //     The main FunctionDecl does not have a mangled name
      //     (mangler.getName(decl) == "") and decl->isDependentContext()
      //     returns true.
      //     
      //  3. Function instantiations.  These are also represented by a
      //     FunctionDecl, and are the declarations referred to by a call.
      //     Template specialization kind can be anything except for
      //     "undeclared" for these FunctionDecl's, which allows us to
      //     distinguish them from calls to non-template functions at the call
      //     site.
      //
      //     Implicit instantiations with the same template parameters share a
      //     single FunctionDecl.
      //
      //     Function instantiations *do* have a mangled name.
      //
      //  A method of a class template has a main function and function
      //  instantiations, but no function template, because the template belongs
      //  to the class, not to the function.
      //
      //  The isTemplated() predicate returns true for the main functions for
      //  both function templates and methods of a class template. It returns
      //  false for instantiations.
      //
      // Strategy:
      //
      // 1. If a function has a function template, emit only the function
      //    template.  This will ensure that template parameters are nested in
      //    the function. Do not emit the main function.
      //
      // 2. If a function does not have a function template (e.g. a plain
      //    function, or a method of a class template), emit the main
      //    function. In this case, there are no template parameters to be
      //    nested.
      //
      // 3. A use that is also an implicit instantiation provides a weak
      //    definition of the instantiated function for linking purposes.
      //
      // 4. Any use of an instantiation - implicit or not - emits a use of the
      //    function template if there is one, or of the main function
      //    otherwise, for cross reference purposes.
      //
      trace_nest("emit function: " << ekind_if_new << " " << okind
        << " for " << decl << " at " << location(loc)
      );
      assert(get_entity_kind(decl) == ekind_if_new);
      trace("function decl specialization kind: "
        << decl->getTemplateSpecializationKind()
        << " templated=" << decl->isTemplated()
      );
      //trace("name: " << decl->getName()); // Only works for simple identifiers
      trace("name as string: " << decl->getNameAsString());
      trace("decl name: " << decl->getDeclName().getAsString());
      if (okind == OccurrenceKind_use && decl->getTemplateSpecializationKind()
        == clang::TSK_ImplicitInstantiation
      ) {
        // A function use that is also an implicit instantiation.  If the symbol
        // is global, then this is relevant for the linker.  Otherwise, e.g. for
        // a static function, we can safely ignore it.
        auto symbol = get_symbol(decl, ekind_if_new);
        if (symbol->is_global()) {
          trace("provide weak definition of " << *symbol);
          analyzer->provide_weak_definition(symbol, analyzer->get_section());
        }
      }
      clang::NamedDecl *target = decl;
      if (auto instantiated_function = decl->getTemplateInstantiationPattern()) {
        trace("instantiated function: " << instantiated_function);
        trace("orig decl: " << mangler.getName(decl));
        ekind_if_new = EntityKind_global_function_template;
        if (auto function_template
          = instantiated_function->getDescribedFunctionTemplate()
        ) {
          // This is an instantiation of a function template.
          trace("function template: " << function_template);
          target = function_template;
        } else {
          // This is an instantiation of a function in a templated context,
          // e.g. a method of a class template.
          target = instantiated_function;
        }
      }
      emit(target, loc, ekind_if_new, okind, is_implicit);
    }
    
    void emit_variable(
      clang::VarDecl *decl
      , clang::SourceLocation loc
      , EntityKind ekind_if_new
      , OccurrenceKind okind
    )
    {
      trace_nest("emit variable: " << ekind_if_new << " " << okind
        << " for " << decl << " at " << location(loc)
      );
      trace("var decl spec kind: " << decl->getTemplateSpecializationKind()
        << " " << decl->isTemplated()
      );
      assert(get_entity_kind(decl) == ekind_if_new);
      if (okind == OccurrenceKind_use && decl->getTemplateSpecializationKind()
        == clang::TSK_ImplicitInstantiation
      ) {
        // A variable use that is also an implicit instantiation.  If the symbol
        // is global, then this is relevant for the linker.  Otherwise, e.g. for
        // a static variable, we can safely ignore it.
        auto symbol = get_symbol(decl, ekind_if_new);
        if (symbol->is_global()) {
          trace("provide weak definition of " << *symbol);
          analyzer->provide_weak_definition(symbol, analyzer->get_section());
        }
      }
      clang::NamedDecl *target = decl;
      if (auto instantiated_variable = decl->getTemplateInstantiationPattern()) {
        trace("instantiated variable: " << instantiated_variable);
        ekind_if_new = EntityKind_global_variable_template;
        if (auto variable_template
          = instantiated_variable->getDescribedVarTemplate()
        ) {
          // This is an instantiation of a variable template.
          trace("variable template: " << variable_template);
          target = variable_template;
        } else {
          // This is an instantiation of a variable in a templated context,
          // e.g. a member of a class template.
          target = instantiated_variable;
        }
        target = instantiated_variable;
      }
      emit(target, loc, ekind_if_new, okind);
    }
    
    void emit(
      clang::NamedDecl *decl
      , clang::SourceLocation loc
      , EntityKind ekind_if_new
      , OccurrenceKind okind
      , bool is_implicit = false
    )
    {
      size_t size = get_name_size(decl, is_implicit);
      emit(get_symbol(decl, ekind_if_new), loc, size, ekind_if_new, okind);
    }

    // Note: size can be zero, for implicit occurrences like the call of a
    // default constructor.
    void emit(
      base::ptr<Symbol> symbol
      , clang::SourceLocation loc
      , size_t name_size
      , EntityKind ekind_if_new
      , OccurrenceKind okind
    )
    {
      trace_nest("emit " << ekind_if_new << " " << okind << " for " << *symbol
        << " at " << location(loc)
      );
      
      // Location comparison.  File loc may actually be what we want, except
      // that we also want to know whether we have a spelling loc or not.
      trace("spelling  loc: " << location(get_spelling_loc(loc)));
      trace("expansion loc: " << location(get_expansion_loc(loc)));
      trace("file      loc: " << location(sm.getFileLoc(loc)));
      clang::SourceLocation occurrence_loc = get_spelling_loc(loc);
      bool no_spelling = !is_in_file(occurrence_loc);
      if (no_spelling) {
        // The name may have been constructed by the preprocessor using
        // concatenation. We will provide the expansion location as an
        // alternative.  This means that the user will not actually see the name
        // of the occurrence at that location, and automatic refactoring
        // (renaming) would fail. We may need to pass an extra flag to the
        // occurrence in the future.
        occurrence_loc = get_expansion_loc(loc);
      }
      base::ptr<File> file = get_file(occurrence_loc);
      
      Range range = get_range(occurrence_loc, name_size);
      trace("emit " << okind << " of " << *symbol << " (" << ekind_if_new
        << ") at " << file->get_name() << " " << range
      );
      Section *section = analyzer->get_section();
      Occurrence *occurrence = analyzer->add_occurrence(
        symbol, ekind_if_new, okind, section, file, range, false
      );
      register_occurrence(occurrence, section);
    }

    clang::NamedDecl *get_symbol_key(clang::NamedDecl *decl)
    {
      return decl->getMostRecentDecl();
    }
    
    base::ptr<Symbol> find_symbol_by_key(clang::NamedDecl *key)
    {
      auto it = _symbol_map.find(key);
      if (it != _symbol_map.end()) {
        return it->second;
      }
      return 0;
    }

    void set_symbol_by_key(clang::NamedDecl *key, base::ptr<Symbol> symbol)
    {
      _symbol_map.insert(std::make_pair(key, symbol));
    }
      
    base::ptr<Symbol> get_symbol(
      clang::NamedDecl *decl, EntityKind ekind_if_new
    )
    {
      clang::NamedDecl *key = get_symbol_key(decl);
      base::ptr<Symbol> symbol = find_symbol_by_key(key);
      if (symbol) {
        trace("reuse " << *symbol);
      } else {
        trace("new symbol " << ekind_if_new << " for " << decl);
        if (is_global_symbol_kind(ekind_if_new)) {
          // UID for global symbol is based on the mangled name.  Mangled name
          // must be obtained from the most recent decl, as that may be a global
          // function while the decl itself is templated and therefore has no
          // mangled name. See comments in get_entity_kind(clang::FunctionDecl).
          std::string link_name = mangler.getName(decl->getMostRecentDecl());
          //std::string link_name = mangler.getName(decl);
          trace(decl->getQualifiedNameAsString() << ": "
            << decl << " " << location(decl) << " = " << link_name
          );
          // The mangled name is empty for the main function decl of a
          // function template. However, the main function template should never
          // be emitted as a global symbol.
          assert_(!link_name.empty(), ekind_if_new << " "
            << decl->getQualifiedNameAsString() << ": "
            << decl << " " << location(decl)
          );
          symbol = analyzer->get_global_symbol(link_name);
          trace("got global symbol " << (void*)symbol << " " << *symbol
            << " for " << decl->getQualifiedNameAsString() << " " << link_name
          );
        } else {
          // To do: handle entities with external or internal linkage in C++:
          // classes, enum types and values, namespaces, references and
          // templates can have external linkage. References and function
          // templates can also have internal linkage.
          //
          // For entities without linkage, location of first declaration is part
          // of UID.
          FileLocation uid_location = get_spelling_location(decl);
          static std::string const anonymous = "(anonymous)";
          const std::string name = get_full_name(decl);
          const std::string &user_name = name.empty() ? anonymous : name;
          trace("user name: " << user_name);
          symbol =
            analyzer->get_local_symbol(ekind_if_new, user_name, uid_location);
          trace("got local symbol " << (void*)symbol << " " << *symbol);
        }
        set_symbol_by_key(key, symbol);
      }
      return symbol;
    }

    bool has_explicit_constructor(clang::CXXRecordDecl *decl)
    {
      for (clang::CXXConstructorDecl *ctor: decl->ctors()) {
        trace("ctor: " << ctor->getType() << " " << ctor->isImplicit());
        if (!ctor->isImplicit()) return true;
      }
      return false;
    }

    void require_class(clang::CXXRecordDecl *decl)
    {
      trace_nest("require class " << decl);
      for (const clang::CXXBaseSpecifier &base: decl->bases()) {
        trace("found base class " << base.getType());
        require_class(base.getType().getTypePtr()->getAsCXXRecordDecl());
      }
      for (clang::CXXMethodDecl *method: decl->methods()) {
        // 
        if (method->isVirtual() && !method->isPure()
          // Don't require a definition for a compiler generated implementation
          // of a virtual destructor or similar. Such a definition is not known
          // in the source analyzer, so would result in a linker failure.  There
          // would be no error locations associated with such a linker failure,
          // because the use of a compiler generated destructor or similar is
          // also implicit, which makes the linker failure confusing.
          && method->isUserProvided()
        ) {
          auto symbol = get_symbol(method, EntityKind_virtual_function);
          // Virtual methods of a class in an anonymous namespace have internal
          // linkage; they can never be called from another compilation unit,
          // and are not global.  For such methods, we should *not* require a
          // global definition.
          if (symbol->is_global()) {
            trace("require definition for virtual method " << symbol->get_name()
              << " in section " << *analyzer->get_section()
            );
            analyzer->require_definition(symbol, analyzer->get_section());
          }
        }
      }
    }
  };

  class ClangDiagnosticHandler
    : public ClangLocator
    , public clang::DiagnosticConsumer
  {
  public:
    size_t error_count = 0;
    ClangDiagnosticHandler(Clang *analyzer
      , const CompilerKind compiler_kind
      , unsigned compiler_version
    ):
      ClangLocator(analyzer)
      , compiler_kind(compiler_kind)
      , compiler_version(compiler_version)
    {
      trace("ClangDiagnosticHandler create");
    }

    ~ClangDiagnosticHandler()
    {
      trace("ClangDiagnosticHandler destroy");
    }

    void HandleDiagnostic(
      clang::DiagnosticsEngine::Level level,
      const clang::Diagnostic &diagnostic
    ) override
    {
      trace("ClangDiagnosticHandler handle " << level);
      clang::DiagnosticConsumer::HandleDiagnostic(level, diagnostic);
      if (level >= clang::DiagnosticsEngine::Warning) {
        llvm::SmallString<64> buffer;
        diagnostic.FormatDiagnostic(buffer);
        std::string message = static_cast<std::string>(buffer);
        trace("message: " << message);
        if (is_bogus_diagnostic(message)) {
          trace("Ignore bogus message: " << message);
          return;
        }
        Severity severity =
          level == clang::DiagnosticsEngine::Warning ?
          Severity_warning : Severity_error;
        bool is_linker_relevant = is_linker_relevant_error(severity, message);
        trace("has source manager: " << diagnostic.hasSourceManager());
        if (diagnostic.hasSourceManager()) {
          // This diagnostic is located somewhere in a source file.
          clang::SourceLocation loc = diagnostic.getLocation();
          clang::SourceManager &sm = diagnostic.getSourceManager();
          trace("is in file: " << is_in_file(loc, sm));
          if (is_in_file(loc, sm)) {
            FileLocation location = get_location(loc, sm);
            trace("Report diagnostic: " << message);
            analyzer->report_diagnostic(message,
              severity, location, is_linker_relevant
            );
            if (severity == Severity_error) {
              ++error_count;
            }
          }
        } else {
          // This diagnostic is not located in a source file.  It could be
          // related to command line parameters, in which case we should ideally
          // report it where the command line is defined, i.e. in the
          // makefile. For now, report it at the start of the source file.
          analyzer->report_diagnostic(message,
            severity, analyzer->unit->file, Location(), is_linker_relevant
          );
          if (severity == Severity_error) {
            ++error_count;
          }
        }
      }
    }

  private:
    // Some compilers have a different behavior than others. As we want to mimic
    // compiler behavior here, we need to know what kind of compiler is in use.
    CompilerKind compiler_kind;
    unsigned compiler_version;

    // Return true iff this is a bogus diagnostic, i.e. a diagnostic detected by
    // Clang but not by the target compiler.
    bool is_bogus_diagnostic(std::string const &message)
    {
      {
        // Regex must match the whole diagnostic message.
        //
        static std::regex bogus_diagnostics_regex(
          // "unknown register name 'vfpcc' in asm": this is caused by a change
          // in ASM syntax between ARMC5 and ARMC6. 'vfpcc' is accepted by gcc
          // 9.2.1 but not by Clang 9.0.1.  Changing the source code as follows
          // also fixes the problem:
          //
          //     #if defined (__clang__)
          //       __ASM volatile ("VMSR fpscr, %0" : : "r" (fpscr) :);
          //     #else
          //       __ASM volatile ("VMSR fpscr, %0" : : "r" (fpscr) : "vfpcc");
          //     #endif
          //
          // "unknown register name 'x6' in asm": this is a similar problem for
          // which I could not find online documentation.
          //
          // To avoid problems, suppress all 'unknown register name' errors at
          // least until we have decent assembly support.
          "unknown register name '.*' in asm"
          "|"
          // Gcc allows initialization for variable length arrays, but Clang
          // doesn't.  Since we are usually using gcc (for arm), ignore this
          // error.  Better, check which compiler we are using and act
          // accordingly (TODO).
          "variable-sized object may not be initialized"
          "|"
          // Gcc knows some attributes that Clang doesn't know. For Arduino,
          // this includes '__progmem__' and 'externally_visible'
          "unknown attribute '.*' ignored"
          "|"
          // Clang warning options differ from gcc warning options.  Ignore
          // warning options for now.  Consider a more intelligent translation
          // later.
          "unknown warning option '-W.*"
          "|"
          , std::regex::ECMAScript | std::regex::optimize
        );
        if (regex_match(message, bogus_diagnostics_regex)) return true;
      }

      if (compiler_kind == CompilerKind_AVR) {
        // Match bogus diagnostics for Arduino's toolchain only.
        static std::regex bogus_diagnostics_regex(
          // Cast from pointer to smaller type is accepted avr-gcc 7.3.0, and
          // such casts are used in macros defined in toolchain headers.  For
          // other compilers including Clang 9.0.1, these casts trigger an error
          // by default, although the error can be transformed into a warning
          // with appropriate command line options for gcc.  According to the
          // C++ standard, behavior of such a cast is implementation defined.
          "cast from pointer to smaller type '.* loses information"
          "|"
          // Assembly code parsing for AVR is incomplete in LLVM 9 - 11.
          "invalid (in|out)put constraint '.*' in asm"
          "|"
          // Assembly code parsing for AVR is buggy in LLVM 11. See
          //   http://avr.2057.n7.nabble.com/
          //     bug-58862-avr-wdt-h-LLVM11-error-for-atmega328p-td23379.html
          "value '64' out of range for constraint 'I'"
          //
          , std::regex::ECMAScript | std::regex::optimize
        );
        if (regex_match(message, bogus_diagnostics_regex)) return true;
      }
  
      return false;
    }
      
    // Determine if this is a linker relevant error, i.e. an error that affects
    // the list of global symbols defined in this translation unit.
    static bool is_linker_relevant_error(
      sa::Severity severity,
      const std::string &message
    )
    {
      // Currently, we assume that any error is linker relevant unless it
      // matches a regular expression for known non-linker-relevant errors. The
      // regular expression may need to be extended in the future.
      static std::regex safe_errors_regex(
        "implicit declaration of function '.*' is invalid in C99"
        "|"
        "unknown register name '\\w+' in asm"
        "|"
        "unused variable '.*'"
        "|"
        "unused function '.*'"
        "|"
        // Atmosic has some examples with an "overlay" file.  Atmosic's overlay
        // file is a header file that is included via a statement similar to
        //
        //   #include STR(OVERLAY)
        //
        // where OVERLAY is a preprocessor macro defined on the command line (in
        // the makefile).  It is only defined in examples that actually use the
        // overlay file. In other examples, the header is not included,
        // resulting in the use of undeclared identifiers.  For correct
        // operation of the source analyzer, it is therefor essential that these
        // errors are not interpreted as linker-relevant.  That will allow the
        // source analyzer to ignore such files.
        //
        // The use of an undeclared identifier is usually not
        // linker-relevant. The front-end will usually substitute a suitable
        // value and continue parsing, detecting all global symbols in the
        // translation unit.
        "use of undeclared identifier '.*'"
        "|"
        // Following error affects only functions called,  not functions defined
        "call to undeclared function '.*';"
        " ISO C99 and later do not support implicit function declarations"
        "|"
        "excess elements in array initializer"
        "|"
        // User-defined errors, generated with #error, are usually not linker
        // relevant, although it is hard to tell in general.
        //
        // For Atmosic, keyboard_hid.c generates an error "Incorrect
        // MAX_ROW/MAX_COL setting" when analyzed in the context of an example
        // that doesn't support the keyboard. The error can be safely ignored
        // when the file is not needed.
        "\".*\""
        "|"
        "no previous prototype for function '.*'"
        "|"
        "result of comparison of constant .* with expression of type '.*'"
        "( \\(aka '.*'\\))?"
        " is always (true|false)"
        "|"
        "implicit conversion from '.*' to '.*'"
        "( \\(aka '.*'\\))?"
        " changes value from .* to .*"
        "|"
        "incomplete definition of type '.*'"
        "|"
        "declaration of '.*' will not be visible outside of this function"
        "|"
        "unknown type name '.*'"
        "|"
        "taking the absolute value of unsigned type '.*'"
        "( \\(aka '.*'\\))?"
        " has no effect"
        "|"
        "initializing '.*'"
        "( \\(aka '.*'\\))?"
        " with an expression of type '.*'"
        "( \\(aka '.*'\\))?"
        " discards qualifiers"
        "|"
        "passing '.*' "
        "(\\(aka '.*'\\) )?"
        "to parameter of type '.*'"
        "( \\(aka '.*'\\))?"
        " discards qualifiers"
        "|"
        "incomplete type '.*'"
        "( \\(aka '.*'\\))?"
        " is not assignable"
        "|"
        "variable has incomplete type '.*'"
        "|"
        "invalid application of '.*' to an incomplete type '.*'"
        "|"
        "no member named '.*' in '.*'"
        "|"
        "cannot assign to non-static data member '.*'"
        " with const-qualified type '.*'"
        "( \\(aka '.*'\\))?"
        "|"
        "expected expression"
        "|"
        "field designator '.*' does not refer to any field in type '.*'"
        "( \\(aka '.*'\\))?"
        "|"
        "pointer/integer type mismatch in conditional expression"
        " \\('.*' and '.*'\\)"
#if 0
        "|"
        ""
        "|"
        ""
        "|"
        ""
        "|"
        ""
        "|"
        ""
#endif
        //
        , std::regex::ECMAScript | std::regex::optimize
      );
      bool relevant = severity >= sa::Severity_error
        // regex_match returns true iff the regex matches the entire message
        && !regex_match(message, safe_errors_regex);
      trace("relevant=" << relevant << " " << message);
      return relevant;
    }
  };
}

sa::Clang::Clang(Unit *unit,
  const std::string &compiler,
  const std::string &resource_path
): Analyzer(unit)
 , source_file(unit->file)
 , resource_path(resource_path)
 , compiler(compiler)
{
  EB_suppress_output();
}

static char get_optimization_level(const std::vector<const char *> &args)
{
  char level = '0';
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i][0] == '-' && args[i][1] == 'O') {
      if (args[i][2]) {
        level = args[i][2];
      } else if (i+1 < args.size()) {
        level = args[i+1][0];
        ++i;
      }
    }
  }
  trace("optimization level: " << level);
  return level;
}

static bool ignore_flag(const char *flag, base::ptr<sa::File> const& file)
{
  if (file->file_kind == sa::FileKind_cplusplus) {
    if (!strcmp(flag, "-std=gnu99")) {
      // gcc ignores -std=gnu99 for C++ files,  but clang gives an error
      return true;
    }
  }
  return false;
}

sa::Clang::~Clang()
{
  trace_nest("~Clang " << (void*)this);
}

bool sa::Clang::run(const std::string &flag_buffer)
{
  assert(base::is_valid(source_file));
  trace_nest("Clang::run " << source_file->get_name());
  base::Timer::Scope scope(analysis_timer);
  bool success = false;

  auto source_path = source_file->get_path();
  if (!base::is_file(source_path)) {
    report_diagnostic("Source file does not exist",
      Severity_error, source_file, Location(), true
    );
  } else if (!base::is_readable(source_path)) {
    report_diagnostic("Source file is not readable",
      Severity_error, source_file, Location(), true
    );
  } else {
    create_section("");
  
    trace("analysis flags for " << source_path);
    const char *extra_flags[] = {
        // arg0
        "clang",
    
        // Clang defines __clang__ which can be used to compile Clang-specific
        // code. This causes problems with some projects that behave differently
        // under Clang.  For now, we assume a gcc-based toolchain, so undefine
        // __clang__.
        "-U__clang__",
        
        // Clang predefines _GNU_SOURCE to 1 for C++ code. Some versions of g++
        // also do that, and some don't.  To be compatible with those that
        // don't, remove Clang's initial definition.  It will be added again by
        // the given flags when needed.
        "-U_GNU_SOURCE",

        // The resource dir contains the Clang-specific system header files.
        // These are required and must match the Clang version in use. They will
        // include the toolchain headers when needed.
        "-resource-dir", resource_path.data(),

        // Be more gcc-like: don't complain about narrowing of non-constant
        // expressions. Message from Clang is "non-constant-expression cannot be
        // narrowed from type 'int' to 'uint8_t' (aka 'unsigned char') in
        // initializer list"
        "-Wno-c++11-narrowing",

        // In contrast to gcc, Clang uses 4 byte enums by default, regardless of
        // the number of bytes actually needed.  Change that for compatibility
        // with gcc.  Enum size is important, as it may affect struct layout,
        // which may cause static asserts to fail (e.g. Atmosic).
        "-fshort-enums",
    
        // Remove the error limit.
        "-ferror-limit=0",

        // Options that are known to clang-cl, but not currently supported, are
        // ignored with a warning. For example:
        //
        //    warning: argument unused during compilation: '--param -fno-rtti'
        //
        // Suppress warnings about unused arguments.
        "-Qunused-arguments",

        // On Windows, turn warnings into errors for #include's with a
        // difference in case between the file name in the #include and the file
        // name on disk.
        "-Werror=nonportable-include-path",
        "-Werror=nonportable-system-include-path",
    };
    const size_t nr_extra_flags = sizeof(extra_flags) / sizeof(*extra_flags);

    std::vector<const char *> args;

    // Add the extra flags first,  so that they can be overruled by user flags.
    for (unsigned i = 0; i < nr_extra_flags; i++) {
      //trace("add extra flag " << extra_flags[i]);
      //trace(extra_flags[i] << " \\");
      args.push_back(extra_flags[i]);
    }
    const char *end = flag_buffer.data() + flag_buffer.size();
    for (const char *flag = flag_buffer.data(); flag < end;
         flag += strlen(flag) + 1
    ) {
      if (!ignore_flag(flag, source_file)) {
        //trace("add flag " << flag << " at " << (nr_extra_flags + i));
        //trace(flag << " \\");
        args.push_back(flag);
      }
    }
    // Put the source file after command_line_args otherwise if '-x' flag is
    // present it will be unused.
    args.push_back(source_path.data());
    trace(source_path << " clang command:\n"
      << base::quote_command_line(args) << "\n\n"
    );
    index_timer.start();
    trace("index source file ...");

    CompilerKind compiler_kind = get_compiler_kind(compiler);
    unsigned compiler_version = get_compiler_version(compiler);

    // Diagnostic handling. Hints:
    //
    // Diagnostics without location are not of interest for us. These could be
    // for example related to command line parameters such as defines.
    //
    // if (is_bogus_diagnostic(message)) ...
    //
    // When Clang issues the following error:
    //
    // 'Arduino.h' file not found with <angled> include; use "quotes" instead
    //
    // then it also includes the file found in the directory of the includer,
    // without giving any further indication that something is wrong.  This
    // causes problems later, not because the file is included, but because it
    // is not added to the missing headers, so there will be no re-analysis
    // when the included file becomes available later.
    //
    // The only work-around I found is to pattern-match to detect these errors
    // and add the mentioned file to the list of missing headers.
    //
    //    static std::regex regex(
    //        "'(.*)' file not found with <angled> include;"
    //        " use \"quotes\" instead"
    //        , std::regex::ECMAScript | std::regex::optimize
    //    );
    //    std::smatch results;
    //    if (regex_match(message, results, regex)) {
    //      assert(results.size() == 2);
    //      trace("missing header from diagnostic: " << results[1]);
    //      report_missing_header(results[1]);
    //    }
    //
    // Hints: check ASTUnit::ConfigureDiags in ASTUnit.cpp
    clang::IntrusiveRefCntPtr<clang::DiagnosticsEngine> diagnostics_engine =
      clang::CompilerInstance::createDiagnostics(new clang::DiagnosticOptions);
    clang::CreateInvocationOptions invocation_options;
    invocation_options.Diags = diagnostics_engine;
    ClangDiagnosticHandler diagnostic_handler(
      this, compiler_kind, compiler_version
    );
    diagnostics_engine->setClient(&diagnostic_handler, false);
    std::shared_ptr<clang::CompilerInvocation> compiler_invocation =
      clang::createInvocation(args, std::move(invocation_options));
    if (!compiler_invocation) {
      // An error should have been reported, probably related to the command
      // line arguments.
      assert(diagnostic_handler.error_count);
    } else if (compiler_invocation->getFrontendOpts().Inputs.empty()) {
      report_diagnostic(
        "Source analysis internal error: empty front-end options",
        Severity_fatal, source_file, Location(), true
      );
    } else {
      debug_string_context("indexing source file with Clang",
        unit->process_name()
      );
    
      // Disable spell checking.  For broken source code, spell-checking can have
      // a significant negative impact on performance, particularly when
      // precompiled headers are involved.
      compiler_invocation->getLangOpts()->SpellChecking = false;
      clang::PreprocessorOptions &PPOpts
        = compiler_invocation->getPreprocessorOpts(); 
      PPOpts.AllowPCHWithCompilerErrors = true;
      PPOpts.DetailedRecord = true; // do we still need to keep a record?

      // Note: ASTUnit constructors are private,  so must use the static 'create'
      // method or variant to create an ASTUnit.
      //
      // The create method:
      //
      //  - sets the diagnostics engine to capture diagnostics (we will overrule
      //     it in the next line with our own diagnostics handler);
      //
      //  - creates a VFS, copies file system options from the compiler
      //    invocation object and create a corresponding file manager
      // 
      //  - grabs a pointer to the compiler invocation from the passed-in shared
      //    pointer;
      //
      //  - creates a source manager using the file manager;
      //
      //  - creates a module cache.
      //
      clang::ASTUnit *ast = clang::ASTUnit::create(
        compiler_invocation,
        diagnostics_engine,
        clang::CaptureDiagsKind::All,
        /*UserFilesAreVolatile=*/true
      ).release();
      diagnostics_engine->setClient(&diagnostic_handler, false);

      // To be able to insert our own preprocessor callbacks, we need an
      // auxiliary "front end action" object and override its
      // "CreateASTConsumer" method.  Trying to access the preprocessor directly
      // - e.g. using ast->getPreprocessor() - fails because the preprocessor is
      // not yet created (null pointer).
      class ClangFrontEndAction: public clang::ASTFrontendAction {
        Clang *analyzer;
      public:
        ClangFrontEndAction(Clang *analyzer): analyzer(analyzer)
        {
          trace("ClangFrontEndAction create");
        }

        ~ClangFrontEndAction()
        {
          trace("ClangFrontEndAction destroy");
        }
      
        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
          clang::CompilerInstance &compiler_instance,
          clang::StringRef source_file
        ) override
        {
          trace("add preprocessor callbacks");
          assert(compiler_instance.hasSourceManager());
          compiler_instance.getPreprocessor().addPPCallbacks(
            std::make_unique<PreprocessorCallbacks>(
              analyzer,
              compiler_instance.getSourceManager()
            )
          );
          return std::make_unique<clang::ASTConsumer>();
        }
      };
      ClangFrontEndAction frontend_action(this);
      trace("call clang::ASTUnit::LoadFromCompilerInvocationAction");
      if (
        !clang::ASTUnit::LoadFromCompilerInvocationAction(
          std::move(compiler_invocation),
          std::make_shared<clang::PCHContainerOperations>(),
          diagnostics_engine,
          &frontend_action,
          ast,
          true, // Persistent
          resource_path,

          // Whether we only want to see "local" declarations (that did not come
          // from a previous precompiled header). If false, we want to see all
          // declarations.
          false,
      
          clang::CaptureDiagsKind::All,
          0, // PrecompilePreambleAfterNParses
          false, // CacheCodeCompletionResults
          /*UserFilesAreVolatile=*/true
        )
      ) {
        trace("clang::ASTUnit::LoadFromCompilerInvocationAction failed");
        report_diagnostic(
          "Source analysis internal error: AST loading failed",
          Severity_fatal, source_file, Location(), true
        );
      } else {
        trace("clang::ASTUnit::LoadFromCompilerInvocationAction succeeded");
        base::Timer::Scope scope(extract_timer);
        success = true;
        char optimization_level = get_optimization_level(args);
        ClangVisitor(
          this, ast, optimization_level, compiler_kind, compiler_version
        ).traverse();
        
        // If the translation unit has been marked as unsafe to free, just
        // discard it.
        if (ast && !ast->isUnsafeToFree()) {
          delete ast;
        }
      }
    }
    index_timer.stop();
  }
  
  trace("source file indexed");
  return success;
}
