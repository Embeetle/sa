// Copyright 2018-2024 Johan Cockx
#ifndef __sa_Symbol_h
#define __sa_Symbol_h

#include "ManagedEntity.h"
#include "Occurrence.h"
#include "FileLocation.h"
#include "base/ptr.h"
#include "base/MemberList.h"
#include <map>
#include <iostream>

namespace sa {
  class Occurrence;
  class OccurrenceData;
  class Project;

  // A symbol UID uniquely identifies a symbol over all files and compilation
  // units in a project.
  //
  // The UID includes the symbol's name and kind. Unless the symbol is global,
  // the location of its first occurrence is also considered part of the UID.
  //
  // For global symbols,  the name is the link name,  the name used for linking.
  //
  // Local symbols can occur in multiple compilation units when they are
  // declared in a header file.  They can even occur multiple times in the same
  // compilation unit, if the header file is included more than once without
  // protection against multiple inclusions. In the compiled code, these symbols
  // will be distinct, but for the programmer, they will often feel like the
  // same symbol. Ideally, therefore, such local symbols should have a single
  // representation, independent of the compilation units in which they occur.
  // This allows right-clicking on such a symbol to show the occurrences
  // over all compilation units. That is why local symbols are entered in the
  // project symbol table instead of in a compilation unit symbol table.
  //
  // The location of the first occurrence of a local symbol is included in its
  // UID to differentiate between distinct local symbols with the same name.
  //
  // Sharing local symbols over compilation units is not possible if the symbol
  // has a different kind or a different scope in each compilation unit. In that
  // case, we create multiple symbols and multiple occurrences at the same file
  // location. To handle this cases, File::find_occurrence must be able to
  // return multiple occurrences for a single location.
  //
  class Symbol: public ManagedEntity {
  public:
    // The symbol name. For C++, this includes scopes and function signatures.
    std::string const name;
    
    // Create a symbol.
    //
    // Note: the new symbol is not registered in the project. Use project
    // methods to get a registered symbol.
    //
    // The name given here is the user name.  It can include any information
    // that is useful to identify the symbol to the user, like parameter types
    // for a function. This name is not used for linking; linking is based on
    // the uid name.
    //
    Symbol(
      EntityKind kind,
      const std::string &name,
      Project *project
    );

    const char *get_name_data() const { return name.data(); }
    
    bool is_global() const { return is_global_symbol(); }

    // Return true iff this symbol has a non-use occurrence.  For this method,
    // a declaration counts as a definition.
    bool has_definition() const;

    // Get the main occurrence of this symbol: a definition if there is one,
    // otherwise a declaration if there is one, otherwise a use if there is one,
    // otherwise null.
    base::ptr<Occurrence> get_main_occurrence() const;

    // Return a list of all occurrences of a given kind.
    std::vector<base::ptr<Occurrence>> get_occurrences(
      OccurrenceKind kind
    );

    virtual void write(std::ostream &out) const = 0;

    // Override for Entity
    Symbol *as_symbol() override;

  protected:

    // Destroy a symbol. Do not call directly if reference counting is used.
    // Make sure the project is locked; otherwise, another thread might attempt
    // to get this symbol while the destructor is running, causing a crash
    // (assertion failure - attempt to increment reference count from zero).
    ~Symbol();

    // Create and return user data for this symbol using the project's add
    // symbol callback. Override for managed entity.
    void create_user_data() override;

    // Drop user data previously created for this symbol using the project's
    // drop symbol callback. Override for managed entity.
    void delete_user_data() override;

    // Override for Entity
    std::string get_entity_name() const override { return name; }

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
    void track_occurrences_of_entity(
      OccurrenceKindSet occurrence_kinds
    ) override;

    // Add a diagnostic for this symbol at the given occurrence.
    void add_diagnostic(
      Occurrence *occurrence,
      const std::string &message,
      Severity severity
    );

    // Remove the existing diagnostic at the given occurrence.
    void remove_diagnostic(Occurrence *occurrence);

    // List of uses of this symbol (both included and excluded).
    MemberList<Occurrence, &Occurrence::entity_index> uses;

    // List of non-use occurrences of this symbol (both included and excluded).
    MemberList<Occurrence, &Occurrence::entity_index> defs;

    // Set of occurrence kinds for which to track occurrences of this symbol.
    OccurrenceKindSet tracked_occurrence_kinds;
  };
 
  inline std::ostream &operator<<(std::ostream &out, const Symbol &symbol)
  {
    symbol.write(out);
    return out;
  }
}

#endif

