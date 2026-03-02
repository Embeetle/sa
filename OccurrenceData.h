// Copyright © 2018-2026 Johan Cockx
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// SPDX-License-Identifier: GPL-3.0-or-later

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
