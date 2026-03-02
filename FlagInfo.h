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

#ifndef __FlagInfo_h
#define __FlagInfo_h

#include "Diagnostic.h"
#include "base/ptr.h"
#include "base/RefCounted.h"
#include <string>
#include <vector>

namespace sa {
  
  // Auxiliary struct,  collecting flag extraction results for a source file.
  struct FlagInfo: public base::RefCounted
  {
    // The path of the compiler used for this file
    std::string const compiler;

    // Compiler-relevant flags for this file.  These are flags that may affect
    // built-in compiler flags.
    std::vector<std::string> const compiler_flags;

    // Analysis-relevant flags for this file. These are flags that may affect
    // the source code analysis, such as enabling or disabling of diagnostics,
    // preprocessor defines, include paths, etc.
    std::vector<std::string> const analysis_flags;

    // Diagnostics generated during flag extraction.
    std::vector<base::ptr<Diagnostic>> const diagnostics;

    bool const valid;

    // Constructor cannibalizes its arguments!
    FlagInfo(
      std::string &compiler,
      std::vector<std::string> &compiler_flags,
      std::vector<std::string> &analysis_flags
    )
      : compiler(compiler)
      , compiler_flags(compiler_flags)
      , analysis_flags(analysis_flags)
      , valid(true)
    {
    }
    
    // Constructor cannibalizes its arguments!
    FlagInfo(
      std::vector<base::ptr<Diagnostic>> &diagnostics
    )
      : diagnostics(diagnostics)
      , valid(false)
    {
    }
  };

}

#endif
