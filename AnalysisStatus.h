// Copyright 2018-2024 Johan Cockx
#ifndef __AnalysisStatus_h
#define __AnalysisStatus_h

#include <iostream>

namespace sa {

  // Analysis status for source files.
  enum AnalysisStatus {
    // Analysis is not required
    AnalysisStatus_none,

    // Analysis is scheduled
    AnalysisStatus_waiting,
  
    // Analysis is in progress
    AnalysisStatus_busy,
  
    // Analysis is done
    AnalysisStatus_done,
  
    // Analysis failed: file is unreadable or does not exist, analysis crashed,
    // flag extraction failed. Note: an analysis that detects errors did not
    // fail; it only fails when it cannot analyze the source files due to one of
    // the above reasons.
    AnalysisStatus_failed,
  };
  static const char * const AnalysisStatus_names[] = {
    "none", "waiting", "busy", "done", "failed"
  };

  inline std::ostream &operator<<(std::ostream &out, AnalysisStatus status)
  {
    return out << AnalysisStatus_names[status];
  }
}

#endif
