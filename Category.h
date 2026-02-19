// Copyright 2018-2024 Johan Cockx
#ifndef __Category_h
#define __Category_h

#include <iostream>
#include <string>
#include <cstddef>

namespace sa {
  // Diagnostic category enum
  typedef enum {
    Category_none,
    Category_toolchain,
    Category_makefile,
  } Category;

  static const size_t NumberOfCategories = 2;

  static const char * const Category_names[] = {
    "none",
    "toolchain",
    "makefile",
  };

  inline Category category_by_name(const std::string &name) {
    for (size_t category = 0; category < NumberOfCategories; ++category) {
      if (Category_names[category] == name) {
        return static_cast<Category>(category);
      }
    }
    return Category_none;
  }

  inline std::ostream &operator<<(std::ostream &out, sa::Category category)
  {
    return out << Category_names[category];
  }
}

#endif
