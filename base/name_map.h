// Copyright 2018-2023 Johan Cockx
#ifndef __base_index_h
#define __base_index_h

#include <map>
#include <string.h>

namespace base {

  // An index is a collected of objects with a name,  that can be found quickly
  // based on their name.
  //
  // The type of the objects in the collection is a template parameter.  The
  // objects must have a method 'const char *name_data() const' that returns the
  // name as a C-string.
  //
  // Similar functionality can be achieved by using a
  // std::map<std::string,Named*>,  at the cost of duplicating the name.
  template <class Named>
  class index
  {
    struct compare {
      bool operator()(char const *a, char const *b) const {
        return strcmp(a, b) < 0;
      }
    };
    std::map<const char*, Named*, compare> map;
  public:
    Named *find(const char *name) const
    {
      auto it = map.find(name);
      return it == map.end() ? 0 : it->second;
    }
    Named *add(Named *named)
    {
      map[named->name_data()] = named;
    }
    void remove(Named *named)
    {
      map.erase(named->name_data());
    }
  };

}
