// Copyright 2018-2024 Johan Cockx
#ifndef __Scope_h
#define __Scope_h

namespace sa {

  // A scope with nested occurrences.
  //
  // Not all occurrences have an associated scope, so this data is only stored
  // for those occurrences that have one.  To reduce memory usage, it is not
  // stored in the occurrence object itself but in a lookup table (see
  // Unit::_scope_data).
  //
  struct Scope {
    
    // Index of first nested occurrence in occurrence list. This is one beyond
    // the index of the definition associated with the scope.
    size_t index = 0;

    // Number of nested occurrences. Includes all descendants, not just direct
    // children.
    size_t count = 0;
  };
}

#endif
