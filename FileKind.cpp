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

#include "FileKind.h"
#include <string.h>


// GCC file extensions
// http://labor-liber.org/en/gnu-linux/development/index.php?diapo=extensions
//
// Extension 	Meaning
// .h           C header file (not to be compiled or linked.
//              Checked as C when using gcc or C++ when using g++
// .c           C source code which must be preprocessed.
// .i           C source code which should not be preprocessed.
// .ii 	        C++ source code which should not be preprocessed.
// .cc, .cp, .cxx, .cpp, .c++, .C 	C++ source code which must be preprocessed.
// .f, .for, .FOR  Fortran source code which should not be preprocessed.
// .F, .fpp, .FPP  Fortran source code which must be preprocessed (with the
//                 traditional preprocessor).
// .r 	        Fortran source code which must be preprocessed with a RATFOR
//              preprocessor (not included with GCC).
// .s           Assembler code.
// .S           Assembler code which must be preprocessed.
// other        An object file to be fed straight into linking. Any file name
//              with no recognized suffix is treated this way.

static const char *get_extension(std::string_view path)
{
  const char *base = path.data();
  const char *x = base + path.length();
  while (x > base && x[-1] != '.' && x[-1] != '/') x--;
  return x[-1] == '.' ? x : "";
}

static bool is_header_extension(const char *extension)
{
  (void)is_header_extension;
  return strcmp(extension, "h") == 0
    || strcmp(extension, "H") == 0;
}

static bool is_c_extension(const char *extension)
{
  return strcmp(extension, "c") == 0 // C code to be preprocessed
    //|| strcmp(extension, "i") == 0   // C code not to be preprocessed
    ;
}

static bool is_cplusplus_extension(const char *extension)
{
  return strcmp(extension, "C") == 0   // C++ code to be preprocessed
    || strcmp(extension, "cpp") == 0 // C++ code to be preprocessed
    || strcmp(extension, "cxx") == 0 // C++ code to be preprocessed
    || strcmp(extension, "c++") == 0 // C++ code to be preprocessed
    || strcmp(extension, "cp") == 0  // C++ code to be preprocessed
    || strcmp(extension, "cc") == 0  // C++ code to be preprocessed
    //|| strcmp(extension, "ii") == 0  // C++ code not to be preprocessed
    ;
}

static bool is_asm_extension(const char *extension)
{
  return strcmp(extension, "s") == 0 
    || strcmp(extension, "asm") == 0 // Needs -x assembler
    ;
}

static bool is_asm_with_cpp_extension(const char *extension)
{
  return strcmp(extension, "S") == 0
    || strcmp(extension, "ASM") == 0 // Needs -x assembler-with-cpp
    ;
}

// Any extension except for the above is acceptable to gcc for object files and
// archives. We use the traditional extensions, and make them case insensitive
// for consistency with Windows.

static bool is_object_extension(const char *extension)
{
  return strcasecmp(extension, "o") == 0
    || strcasecmp(extension, "obj") == 0
    ;
}

static bool is_archive_extension(const char *extension)
{
  return strcasecmp(extension, "a") == 0;
}

static bool is_executable_extension(const char *extension)
{
  return !*extension || strcasecmp(extension, "exe") == 0;
}

sa::FileKind sa::guess_gcc_file_kind(std::string_view path)
{
  const char *extension = get_extension(path);
  return
    is_c_extension(extension) ? sa::FileKind_c :
    is_cplusplus_extension(extension) ? sa::FileKind_cplusplus :
    is_asm_extension(extension) ? sa::FileKind_assembler :
    is_asm_with_cpp_extension(extension) ? sa::FileKind_assembler_with_cpp :
    is_executable_extension(extension) ? sa::FileKind_executable :
    is_object_extension(extension) ? sa::FileKind_object :
    is_archive_extension(extension) ? sa::FileKind_archive :
    is_header_extension(extension) ? sa::FileKind_header :
    sa::FileKind_other;
}

bool sa::is_header_file(std::string_view path)
{
  return is_header_extension(get_extension(path));
}

bool sa::is_c_file(std::string_view path)
{
  return is_c_extension(get_extension(path));
}

bool sa::is_cplusplus_file(std::string_view path)
{
  return is_cplusplus_extension(get_extension(path));
}

bool sa::is_asm_file(std::string_view path)
{
  return is_asm_extension(get_extension(path));
}

bool sa::is_asm_with_cpp_file(std::string_view path)
{
  return is_asm_with_cpp_extension(get_extension(path));
}

bool sa::is_object_file(std::string_view path)
{
  return is_object_extension(get_extension(path));
}

bool sa::is_archive_file(std::string_view path)
{
  return is_archive_extension(get_extension(path));
}

bool sa::is_executable_file(std::string_view path)
{
  return is_executable_extension(get_extension(path));
}

