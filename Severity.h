// Copyright 2018-2024 Johan Cockx
#ifndef __Severity_h
#define __Severity_h

#include <iostream>
#include <string>
#include <cstddef>

namespace sa {
  // Diagnostic severity enum
  typedef enum {
    // Diagnostic indicates suspicious code that may not be wrong.
    Severity_warning,

    // Diagnostic indicates that the code is ill-formed.
    Severity_error,

    // Diagnostic indicates that analysis failed.
    Severity_fatal,

    // Internal value,  never used for actual diagnostic
    Severity_none,
  } Severity;

  static const char * const Severity_names[] = {
    "warning", "error", "fatal", "none"
  };

  static const size_t NumberOfSeverities = Severity_none;

  inline Severity severity_by_name(const std::string &name) {
    for (int severity = Severity_warning; severity < Severity_none; ++severity) {
      if (Severity_names[severity] == name) {
        return static_cast<Severity>(severity);
      }
    }
    return Severity_none;
  }

  inline std::ostream &operator<<(std::ostream &out, sa::Severity severity)
  {
    return out << Severity_names[severity];
  }
}

#endif
