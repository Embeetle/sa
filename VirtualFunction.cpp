// Copyright 2018-2024 Johan Cockx
#include "VirtualFunction.h"
#include <algorithm>

sa::VirtualFunction::VirtualFunction(
  const SymbolUid &uid,
  const std::string &name,
  Project *project,
  Record *parent,
  std::vector<VirtualFunction*> overridees
)
  : Symbol(uid, name, project)
  , _parent(parent)
  , _overridees(overridees)
{
  parent->_virtual_functions.push_back(this);
  for (auto overridee: _overridees) {
    overridee->_overriders.push_back(this);
  }
}

sa::VirtualFunction::~VirtualFunction()
{
  assert(_overriders.empty());
  for (auto overridee: _overridees) {
    overridee->_overriders.erase(
      std::find(
        overridee->_overriders.begin(),
        overridee->_overriders.end(),
        this
      )
    );
  }
  _parent->_virtual_functions.erase(
    std::find(
      _parent->_virtual_functions.begin(),
      _parent->_virtual_functions.end(),
      this
    )
  );
}
