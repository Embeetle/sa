// Copyright 2018-2024 Johan Cockx
#ifndef __VirtualFunction_h
#define __VirtualFunction_h

#include "Symbol.h"
#include "Record.h"

namespace sa {

  class VirtualFunction: public Symbol
  {
    Record * _parent;
    std::vector<VirtualFunction*> _overridees;
    std::vector<VirtualFunction*> _overriders;

  public:
    
    VirtualFunction(
      const SymbolUid &uid,
      const std::string &name,
      Project *project,
      Record *parent,
      std::vector<VirtualFunction*> overridees
    );

    ~VirtualFunction();

    Record *parent() const { return _parent; }
    
    const std::vector<VirtualFunction*> &overridees() const
    {
      return _overridees;
    }
    
    const std::vector<VirtualFunction*> &overriders() const
    {
      return _overriders;
    }
    
  };

}

#endif

