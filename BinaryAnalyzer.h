// Copyright 2018-2024 Johan Cockx
#ifndef __BinaryAnalyzer_h
#define __BinaryAnalyzer_h

#include "Analyzer.h"

namespace sa {
  class BinaryAnalyzer: public Analyzer {
  public:
    BinaryAnalyzer(Unit *unit);

    ~BinaryAnalyzer();

    // Run analysis of binary file: object file, archive, ... . Return true on
    // success and false on failure.  An analysis that finds errors is still
    // successful.
    bool run(const std::string &flag_buffer) override;

  protected:
    void handle_stdout(std::istream &in);
    void handle_stderr(std::istream &in);

  private:
    base::ptr<File> source_file;
  };
}

#endif

