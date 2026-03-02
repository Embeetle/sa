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

#ifndef __EntityKind_h
#define __EntityKind_h

#include <bitset>
#include <iostream>

namespace sa {
  // Entity kind enum.  When changing anything in this enum, also change entity
  // kind codes in Analyzer.cpp.
  //
  enum EntityKind {
    // Entity kind 'none' is not used on actual entities. It is used as a
    // special return value indicating no entity.
    EntityKind_none,                  //    0

    // Global symbols are entities with a globally unique name for which at most
    // one strong definition is allowed. Typical examples are global functions
    // and variables. They all live in the same namespace: a global variable
    // cannot have the same name as a global function.
    //
    // In C/C++ code, we can always determine whether a global entity is a
    // function or a variable, and we assign it the corresponding entity kind.
    // A global symbol without further qualification can be found in assembly
    // code.  It can be a:
    //  - global variable
    //  - global function
    //  - C++ inline function (weak definition unless all calls are inlined)
    //  - C99 or GNU99 extern inline function
    //  - GNU89 inline function
    //
    // We create separate symbols for a global symbol, a global variable and a
    // global function with the same name. Such symbols will be linked together
    // by the linker.
    //
    EntityKind_global_symbol,         // C
    //
    EntityKind_global_variable,       // V
    EntityKind_global_function,       // F
    EntityKind_virtual_function,      // b
    MaxGlobalEntityKind = EntityKind_virtual_function,

    EntityKind_global_variable_template,     // w
    EntityKind_global_function_template,     // g
    EntityKind_class_template,        // d
    EntityKind_template_parameter,    // h
    
    //
    // For some background, here is a good explanation of types, scope, storage
    // duration and linkage in C and C++, and the corresponding terminology:
    // https://www.embedded.com/linkage-in-c-and-c
    //
    // In C, only functions and objects (variables) have linkage.
    // In C++, also classes, enum types and values, namespaces, references and
    // templates can have external linkage. References and function templates
    // can also have internal linkage.
    //
    // The C++ language standard mandates that all symbols with external linkage
    // have globally unique names, as they are used to construct unique mangled
    // names for (member-) functions and variables. Definitions in different
    // compilation units must be identical.  This rule is currently not checked,
    // except indirectly through the generated member names.
    //
    // Local symbols are only known in a single compilation unit. Different
    // compilation units can have different definitions, which are unrelated.
    //
    // Todo: explain section names and files.
    EntityKind_struct,                // r
    EntityKind_union,                 // u
    EntityKind_enum,                  // n
    EntityKind_section,               // N
    EntityKind_file,                  //  

    EntityKind_local_symbol,          // c  Assembly only
    EntityKind_local_static_variable, // v  Static function-local
    EntityKind_static_variable,       // s  Static unit-local
    EntityKind_automatic_variable,    // a  Stack
    EntityKind_parameter,             // p
    EntityKind_local_function,        // f  Static and/or inline
    //
    EntityKind_type,                  // t
    EntityKind_field,                 // m
    EntityKind_enum_constant,         // e
    EntityKind_macro,                 // M
    //
    // C++ class or struct
    EntityKind_class,                 // l
    
    EntityKind_other,                 // o
    NumberOfEntityKinds,              //  
  };
  
  static const char *const EntityKind_names[] = {
    "none",
    "global symbol",
    "global variable",
    "global function",
    "virtual function",
    "variable template",
    "function template",
    "class template",
    "template parameter",
    "struct",
    "union",
    "enum",
    "section",
    "file",
    "unit-local symbol",
    "local static variable",
    "static variable",
    "automatic variable",
    "parameter",
    "local function",
    "type",
    "field",
    "enum constant",
    "macro",
    "class",
    "other",
  };

  // Unique char codes for caching of symbol kinds
  const char EntityKind_codes[] = " CVFbwgdhrunN cvsapftmeMlo";
  
  inline bool is_symbol_kind(EntityKind kind)
  {
    return kind != EntityKind_file;
  }
  
  inline bool is_global_symbol_kind(EntityKind kind)
  {
    return kind != EntityKind_none && kind <= MaxGlobalEntityKind;
  }
  
  inline bool is_local_symbol_kind(EntityKind kind)
  {
    return !is_global_symbol_kind(kind) && is_symbol_kind(kind);
  }

  // True for all variable kinds
  inline bool is_variable_kind(EntityKind kind)
  {
    return kind == sa::EntityKind_automatic_variable
      || kind == sa::EntityKind_global_variable
      || kind == sa::EntityKind_static_variable
      || kind == sa::EntityKind_local_static_variable
      || kind == sa::EntityKind_global_variable_template
      ;
  }

  // True for kinds whose declarations can contain nested declarations: any
  // function (with parameters and/or template parameters) or
  // variable or class template (with template parameters)
  inline bool declaration_can_be_scope(EntityKind kind)
  {
    return kind == sa::EntityKind_global_symbol
      || kind == sa::EntityKind_local_function
      || kind == sa::EntityKind_global_function
      || kind == sa::EntityKind_virtual_function
      || kind == sa::EntityKind_global_function_template
      || kind == sa::EntityKind_global_variable_template
      || kind == sa::EntityKind_class_template
      ;
  }
  
  // True for kinds whose definitions can contain nested declarations: any
  // function (with parameters, local variables and/or template parameters),
  // variable or class template (with template parameters), macro (with
  // arguments), class/struct/union/enum (with members), variable (with
  // initialization list) or section.
  inline bool definition_can_be_scope(EntityKind kind)
  {
    return declaration_can_be_scope(kind)
      || is_variable_kind(kind)
      || kind == sa::EntityKind_class
      || kind == sa::EntityKind_struct
      || kind == sa::EntityKind_union
      || kind == sa::EntityKind_enum
      || kind == sa::EntityKind_macro
      || kind == sa::EntityKind_section
      ;
  }

  static_assert(
    sizeof(EntityKind_names)/sizeof(*EntityKind_names) == NumberOfEntityKinds
  );
  static_assert(sizeof(EntityKind_codes)-1 == NumberOfEntityKinds);
  
  typedef std::bitset<NumberOfEntityKinds> EntityKindSet;

  inline std::ostream &operator<<(std::ostream &out, sa::EntityKind kind)
  {
    return out << EntityKind_names[kind];
  }
}

#endif
