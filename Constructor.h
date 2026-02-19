// Copyright 2018-2024 Johan Cockx
#ifndef __Constructor_h
#define __Constructor_h

#include "GlobalSymbol.h"
#include "Record.h"

namespace sa {

  class Constructor: public Symbol
  {
    Record * _record;

  public:
    
    Constructor(
      Record *record,
      const std::string &name,
      const std::string &link_name
    );

    ~Constructor();

    Record *record() const { return _record; }
  };

}

#endif

