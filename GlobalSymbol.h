// Copyright 2018-2024 Johan Cockx
#ifndef __GlobalSymbol_h
#define __GlobalSymbol_h

#include "Symbol.h"
#include "LinkStatus.h"

namespace sa {

  // A global symbol is a function or variable that can occur in multiple
  // compilation units. Global functions and variables are resolved by the
  // linker based on their link name, which is a mangled name for C++ functions
  // and variables.
  //
  // A global symbol has external linkage. Symbols with external linkage include
  // global functions and variables as well as C++ classes, enum types and
  // values, namespaces, references and templates.  Of these, only global
  // functions and variables are resolved by the linker. The others are never
  // seen by the linker, but the C++ language specification still requires them
  // to be consistent accross compilation units.  This is in particular
  // important for name mangling / type safe linking.
  //
  // C structs, enum types and values do not have external linkage.  In C++,
  // references and function templates can also have internal linkage (with the
  // "static" keyword).
  //
  // A symbol with the same kind and link-name can occur with different
  // usernames.  This is specifically the case for C functions that can occur
  // with different parameter lists. Currently, only the first user name is
  // kept. TODO: keep a list of names?  Keep the longest name? Or remove
  // parameter lists?  Or demangle the mangled name to use as a user name?
  //
  // Similar problem: symbols with the same link name but different kind are
  // treated as the same symbol by the linker. How to handle this?
  //
  class GlobalSymbol: public Symbol
  {
  public:
    std::string const link_name;
    
    // Create a global symbol.
    //
    // Note: the new symbol is not registered in the project and is not
    // uniquized. Use project methods to get a registered unique symbol.
    //
    GlobalSymbol(
      const std::string &link_name,
      Project *project
    );

    ~GlobalSymbol();

    LinkStatus get_link_status() const { return link_status; }
    
    // Set the link status,  and report if changed.
    void set_link_status(LinkStatus status);

    // Add an "undefined symbol" diagnostic at the given use.
    void add_undefined_diagnostic(Occurrence *use);

    // Add a "multiply defined" diagnostic at the given definition.
    void add_multiply_defined_diagnostic(Occurrence *definition);
    
    void write(std::ostream &out) const override;

    // Override for RefCounted. Remove the symbol from its project and
    // delete it.
    void zero_ref_count() override;

  protected:
    
    // Override for Entity
    void insert_instance_of_entity(
      bool linked,
      Occurrence *occurrence,
      bool is_first_instance,
      bool is_first_linked_instance
    ) override;

    // Override for Entity
    void remove_instance_of_entity(
      bool linked,
      Occurrence *occurrence,
      bool is_last_instance,
      bool is_last_linked_instance
    ) override;

    // Override for Entity
    GlobalSymbol *as_global_symbol() override;

    // Override for Entity
    EntityKind get_effective_kind() const override { return effective_kind; }

    // Recompute the effective kind based on linked occurrence style counts.
    void recompute_effective_kind();

  private:
    LinkStatus link_status = LinkStatus_none;
    EntityKind effective_kind = EntityKind_global_symbol;

    // Number of linked occurrences with specific styles
    unsigned data_count = 0;
    unsigned function_count = 0;
    unsigned virtual_function_count = 0;
  };

  std::ostream &operator<<(std::ostream &out, const GlobalSymbol &symbol);
}

#endif

