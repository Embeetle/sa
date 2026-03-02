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


#ifndef __sa_compiler_h
#define __sa_compiler_h

#include "FileKind.h"
#include "Diagnostic.h"
#include <string>
#include <vector>
#include <tuple>

namespace sa {
  struct CompilerDiagnostic {
    std::string message;
    Severity severity;
    Category category;

    CompilerDiagnostic(
      std::string_view message,
      Severity severity,
      Category category
    ): message(message), severity(severity), category(category)
    {
    }

    bool operator==(CompilerDiagnostic const &other) const {
      return std::tie(message, severity, category) ==
        std::tie(other.message, other.severity, other.category);
    }
    bool operator!=(CompilerDiagnostic const &other) const {
      return ! (*this == other);
    }
    bool operator<(CompilerDiagnostic const &other) const {
      return std::tie(message, severity, category) <
        std::tie(other.message, other.severity, other.category);
    }
    bool operator>(CompilerDiagnostic const &other) const {
      return other < *this;
    }
    bool operator<=(CompilerDiagnostic const &other) const {
      return ! (*this > other);
    }
    bool operator>=(CompilerDiagnostic const &other) const {
      return ! (*this < other);
    }
  };

  struct CompilerResults {
    // Are these compiler results valid, i.e. could the built-in flags be
    // extracted?
    bool valid;
    
    // If valid, built-in compiler flags that are relevant when analyzing a
    // source file with another compiler front-end, such as Clang. The returned
    // flags are patched as required for Clang. Specifically contains macro
    // definitions, system includes and target definition.
    std::vector<std::string> builtin_flags;

    // If not valid, diagnostics related to why the built-in flags could not be
    // extracted.
    std::vector<CompilerDiagnostic> diagnostics;
  };

  // Check if this flag potentially affects built-in compiler macros. For
  // example, -mfloat-abi=hard affects whether __SOFTFP__ is defined as a
  // built-in macro.
  //
  bool is_compiler_builtin_relevant(std::string const &flag);

  // Return built-in compiler flags for a given compiler, source file kind and
  // relevant user-provided flags.
  //
  // Results are obtained by running the compiler with special flags and parsing
  // the output. Results are cached for efficiency. There is no provision to
  // clear the cache, so the returned pointer remains valid indefinitely.
  //
  // Thread-safe; multiple threads can call this at once.
  CompilerResults const *get_compiler_builtin_flags(
    std::string_view compiler,
    FileKind source_kind,
    std::vector<std::string> const &relevant_flags
  );
  
  // Same,  with source path instead of source file kind.
  inline CompilerResults const *get_compiler_builtin_flags(
    std::string_view compiler,
    std::string_view source,
    std::vector<std::string> const &relevant_flags
  )
  {
    return get_compiler_builtin_flags(
      compiler, guess_gcc_file_kind(source), relevant_flags
    );
  }
}

#endif
