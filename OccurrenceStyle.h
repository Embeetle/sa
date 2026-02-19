// Copyright 2018-2024 Johan Cockx
#ifndef __OccurrenceStyle_h
#define __OccurrenceStyle_h

namespace sa {
  // Symbol access kind enum - specifies how an occurrence accesses a symbol.
  enum OccurrenceStyle {
    OccurrenceStyle_unspecified,        // u 0
    OccurrenceStyle_data,               // d 1
    OccurrenceStyle_function,           // f 2
    OccurrenceStyle_virtual_function,   // v 3 
  };
  
  static const char * const OccurrenceStyle_names[] = {
    "unspecified", "data", "function", "virtual function",
  };

  inline std::ostream &operator<<(std::ostream &out, sa::OccurrenceStyle kind)
  {
    return out << sa::OccurrenceStyle_names[kind];
  }
}

#endif
