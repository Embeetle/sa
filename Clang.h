// Copyright 2018-2024 Johan Cockx
#ifndef __Clang_h
#define __Clang_h

#include "File.h"
#include "Analyzer.h"
#include "base/ptr.h"
#include <string>

namespace sa {
  class Clang: public Analyzer {
    base::ptr<File> source_file;
    const std::string resource_path;
    const std::string compiler;

  public:
    Clang(Unit *unit,
      const std::string &compiler,
      const std::string &resource_path
    );
    
    ~Clang();

    // Run Clang analysis. Return true on success and false on failure.
    // An analysis that finds errors in the source code is still successful.
    bool run(const std::string &flag_buffer) override;

    // Currently using a single section.  TODO: separate .text and .data
    // sections, subsections for --function-sections and --data-sections.
  };
}

#endif
