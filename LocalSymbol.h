// Copyright 2018-2024 Johan Cockx
#ifndef __LocalSymbol_h
#define __LocalSymbol_h

#include "Symbol.h"

namespace sa {

  // A local symbol is a symbol that is only known within a compilation unit.
  //
  // If a local symbol is declared or defined in a header, we treat it as the
  // same symbol for all including units on condition that the name, kind,
  // location of the first occurrence and scope match.  This leads to more
  // natural behavior from a user point of view.
  //
  // To decide whether local symbols declared or defined in header are the same,
  // we look at the "reference location" of the symbol.  The reference location
  // can be freely chosen by the code analyzer.  It is typically the location of
  // the first declaration or the definition of the symbol. Note that that is
  // not necessarily the same as the first occurrence of the symbol;
  // specifically for macros, the first occurrence can be a test, like #ifdef or
  // #ifndef.
  //
  // Treating symbols as one if kind, name or scope don't match would complicate
  // our data structure.  It would mean that a symbol can have multiple names,
  // kinds or scopes. We avoid this complication by creating multiple symbols
  // that happen to have the same reference location.
  //
  class LocalSymbol: public Symbol
  {
    // ref_location can change when the file is edited.
    friend class File;
    FileLocation ref_location;
    
  public:
    FileLocation const &get_ref_location() const { return ref_location; };
    base::ptr<Occurrence> const ref_scope;
    
    // Create a local symbol.
    //
    // Note: the new symbol is not registered in the project and is not
    // uniquized. Use project methods to get a registered unique symbol.
    //
    // The name given here is the user name.  It can include any information
    // that is useful to identify the symbol to the user, like parameter types
    // for a function.
    //
    LocalSymbol(
      EntityKind kind,
      const std::string &user_name,
      const FileLocation &ref_location,
      base::ptr<Occurrence> ref_scope,
      Project *project
    );

    ~LocalSymbol();

    bool operator<(const LocalSymbol& other) const;
    bool operator==(const LocalSymbol& other) const;
    bool operator!=(const LocalSymbol& other) const { return !(*this == other); }
    bool operator>(const LocalSymbol& other) const { return other < *this; }
    bool operator<=(const LocalSymbol& other) const { return !(*this > other); }
    bool operator>=(const LocalSymbol& other) const { return !(*this < other); }

    // Override from RefCounted. Remove the local symbol from its project and
    // delete it.
    void zero_ref_count() override;

    void write(std::ostream &out) const override;

    // Override for Entity
    Occurrence *get_ref_scope() const override;

    // Override for Entity
    LocalSymbol *as_local_symbol() override;
  };

  std::ostream &operator<<(std::ostream &out, const LocalSymbol &symbol);

}

template <> struct std::hash<sa::LocalSymbol> {
  std::size_t operator()(const sa::LocalSymbol &symbol) const
  {
    return hash<std::string>()(symbol.name)
      ^ hash<unsigned>()(symbol.kind) << 23
      ^ hash<sa::Location>()(symbol.get_ref_location())
      ^ hash<sa::Occurrence*>()(symbol.ref_scope)
      ;
  }
};

#endif

