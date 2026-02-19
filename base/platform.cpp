// Copyright 2018-2023 Johan Cockx
#include "platform.h"
#include <string>
#include <cxxabi.h>

std::string base::demangle(const char *name)
{
  if (name[0] != '_' || name[1] != 'Z') return name;
  int status;
  char *demangled_name_ptr = abi::__cxa_demangle(name, 0, 0, &status);
  if (status) return name;
  std::string demangled_name = demangled_name_ptr;
  free(demangled_name_ptr);
  return demangled_name;
}

#ifdef SELFTEST

#include <iostream>

int main()
{
  std::cout << "Selftest for platform\n";
  std::cout << base::demangle("a") << "\n";
  std::cout << "Bye\n";
  return 0;
}

#endif

