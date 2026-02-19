// Copyright 2018-2023 Johan Cockx
#ifndef __base_compare_c_strings_h
#define __base_compare_c_strings_h

#include <string.h>

namespace base {

  // Aux object to compare C strings, usable as third template parameter for
  // std::map and std::set.

  struct compare_c_strings {
    bool operator()(char const *a, char const *b) const {
      return strcmp(a, b) < 0;
    }
  };
}

#endif
