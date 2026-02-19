// Copyright 2018-2024 Johan Cockx
#include "Constructor.h"
#include <algorithm>

sa::Constructor::Constructor(
  Record *record,
  const std::string &name,
  const std::string &link_name
)
  : Symbol(name, project)
  , _parent(parent)
{
}

sa::Constructor::~Constructor()
{
}
