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

#ifndef __FileKind_h
#define __FileKind_h

#include <iostream>
#include <string>

namespace sa {
  
  // Kind enum for source files.
  enum FileKind {
    // Unrecognized file
    FileKind_other,

    // Executable file
    FileKind_executable,
    
    // Header file, to be included in a C/C++ file
    FileKind_header,

    // Archive file
    FileKind_archive,

    // Object file
    FileKind_object,

    // Assembler file
    FileKind_assembler,
    
    // Assembler file with C preprocessing
    FileKind_assembler_with_cpp,
    
    // C source file
    FileKind_c,

    // C++ source file
    FileKind_cplusplus,
  };

  static const char * const FileKind_names[] = {
    "other",
    "executable",
    "header",
    "archive",
    "object",
    "assembler",
    "assembler-with-cpp",
    "C",
    "C++"
  };

  static inline std::ostream &operator<<(std::ostream &out, FileKind kind)
  {
    return out << FileKind_names[kind];
  }

  // Files with a source kind need to be compiled before linking. Compilation
  // flags are relevant.
  static inline bool is_source_file_kind(FileKind kind)
  {
    return kind >= FileKind_assembler;
  }

  // Source files with a preprocessed file kind need to be preprocessed. Hdirs
  // and other preprocessor options are relevant.
  static inline bool is_preprocessed_file_kind(FileKind kind)
  {
    return kind >= FileKind_assembler_with_cpp;
  }

  // Files with a linkable kind include archives and object files in addition to
  // source files.
  static inline bool is_linkable_file_kind(FileKind kind)
  {
    return kind >= FileKind_archive;
  }

  // Guess file kind the same way gcc compilers guess, based on file extension.
  FileKind guess_gcc_file_kind(std::string_view path);

  inline bool is_source_file(std::string_view path)
  {
    return is_source_file_kind(guess_gcc_file_kind(path));
  }

  inline bool is_preprocessed_file(std::string_view path)
  {
    return is_preprocessed_file_kind(guess_gcc_file_kind(path));
  }

  inline bool is_linkable_file(std::string_view path)
  {
    return is_linkable_file_kind(guess_gcc_file_kind(path));
  }

  bool is_header_file(std::string_view path);
  bool is_c_file(std::string_view path);
  bool is_cplusplus_file(std::string_view path);
  bool is_asm_file(std::string_view path);
  bool is_asm_with_cpp_file(std::string_view path);
  bool is_object_file(std::string_view path);
  bool is_archive_file(std::string_view path);
  bool is_executable_file(std::string_view path);
};

#endif
