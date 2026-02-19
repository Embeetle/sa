// Copyright 2018-2023 Johan Cockx
#ifndef __base_platform_h
#define __base_platform_h

#include <string>
#include <typeinfo>

namespace base {
  std::string demangle(const char *name);
  inline std::string demangle(const std::string &name)
  {
    return demangle(name.data());
  }
  
  template <typename T> std::string type_name(const T &t)
  {
    return demangle(typeid(t).name());
  }

}

#endif
