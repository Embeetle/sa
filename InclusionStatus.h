// Copyright 2018-2024 Johan Cockx
#ifndef __InclusionStatus_h
#define __InclusionStatus_h

#include <iostream>
#include <bitset>

namespace sa {

  enum InclusionStatus {
    // An excluded entity occurs in excluded and unused source files only.  It
    // will not be compiled and linked with the project's executable when
    // building the project.
    InclusionStatus_excluded,

    // An included entity will be compiled and linked with the project's
    // executable when building the project.
    InclusionStatus_included,

    NumberOfInclusionStatuses
  };
  static const char * const InclusionStatus_names[] = {
    "excluded", "included"
  };
  
  inline std::ostream &operator<<(std::ostream &out, InclusionStatus status)
  {
    return out << InclusionStatus_names[status];
  }

  typedef std::bitset<NumberOfInclusionStatuses> InclusionStatusSet;
}

#endif
