// Copyright 2018-2024 Johan Cockx
#ifndef __sa_status_map_type_h
#define __sa_status_map_type_h

#include "LinkStatus.h"
#include "base/compare_c_strings.h"
#include <map>

namespace sa {
  // Type of link status map for global symbols. Used to report linker results
  // to the project.
  //
  // The key for this map is a C string; there is no need to copy the name of
  // global symbols into a std::string.
  //
#if 0
  typedef std::map<const char *, LinkStatus, base::compare_c_strings>
  link_status_map_type;
#else
  typedef std::map<std::string, LinkStatus> link_status_map_type;
#endif
}

#endif
