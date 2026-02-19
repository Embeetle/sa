// Copyright 2018-2024 Johan Cockx
#ifndef __AsmAnalyzer_h
#define __AsmAnalyzer_h

#include "Analyzer.h"
#include "ExternalAnalyzer.h"

namespace sa {
  class AsmAnalyzer: public ExternalAnalyzer {

  public:
    AsmAnalyzer(Unit *unit);

    ~AsmAnalyzer();

    // Run asm analysis. Return true on success and false on failure.
    // An analysis that finds errors in the source code is still successful.
    bool run(const std::string &flag_buffer) override;

  protected:
    void handle_stderr(std::istream &in) override;
    void handle_asm_error_line(const std::string &line);

    bool is_linker_relevant_diagnostic(
      sa::Severity severity,
      const std::string &message
    ) override;
    
  };
}

#endif
