// Copyright 2018-2024 Johan Cockx
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
