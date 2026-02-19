// Copyright 2018-2024 Johan Cockx
#ifndef __OccurrenceData_h
#define __OccurrenceData_h

#include "OccurrenceKind.h"

namespace sa {
  class Entity;
  
  // Struct describing an occurrence of an entity in a file.
  struct OccurrenceData {
    OccurrenceKind kind;
    void *entity_user_data; // Python object, user handle
    Entity *entity;         // SA handle
    const char *path;
    unsigned begin_offset;  // zero-based, first char of occurrence
    unsigned end_offset;    // zero-based, first char after occurrence
    bool linked;
  };

  // Null occurrence
  static const struct OccurrenceData null_occurrence_data = {
    OccurrenceKind_none, 0, 0, 0, 0, 0, false
  };

}

#endif
