// Copyright 2018-2024 Johan Cockx
#ifndef __LinkStatus_h
#define __LinkStatus_h

#include <iostream>

namespace sa {

  // Link status for a symbol.
  enum LinkStatus {
    // The symbol is not involved in linking, either because it is not directly
    // or indirectly used from any of the program's entry points, or because it
    // is a local symbol.
    LinkStatus_none,

    // The symbol is undefined and only weakly used; if no definition is found,
    // it defaults to zero without error.
    LinkStatus_weakly_undefined,

    // The symbol is used and has no definitions
    LinkStatus_undefined,

    // The symbol is used, has no strong or tentative definition and has one or
    // more weak definitions.
    LinkStatus_weakly_defined,

    // The symbol is used, has no strong definition and has one or more tentative
    // (=common) definitions.
    LinkStatus_tentatively_defined,

    // The symbol is used and has exactly one strong definition
    LinkStatus_defined,

    // The symbol is used and has more than one strong definition
    LinkStatus_multiply_defined,
  };
  
  static const char * const LinkStatus_names[] = {
    "none", "weakly undefined", "undefined", "weakly defined",
    "tentatively defined", "defined", "multiply defined"
  };

  inline bool is_defined(LinkStatus status)
  {
    return status > LinkStatus_undefined;
  }

  inline std::ostream &operator<<(std::ostream &out, LinkStatus status)
  {
    return out << LinkStatus_names[status];
  }
}

#endif
