// Copyright 2018-2024 Johan Cockx
#ifndef __sa_Location_h
#define __sa_Location_h

#include <iostream>

namespace sa {

  // A location in a source file.
  struct Location {

    // Zero-based byte offset from start of file.
    unsigned offset;
    
    Location(): offset(0) {}

    Location(unsigned offset): offset(offset)
    {
    }

    // Location is usable as key for std::set and std::map
    bool operator<(const Location& other) const
    {
      return offset < other.offset;
    }

    // Location is usable as key for std::unordered_set and std::unordered_map
    // A hash function is defined below, near the end of this file.
    bool operator==(const Location& other) const
    {
      return offset == other.offset;
    }

    bool operator!=(const Location& other) const { return !(*this == other); }
    bool operator>(const Location& other) const { return other < *this; }
    bool operator<=(const Location& other) const { return !(*this > other); }
    bool operator>=(const Location& other) const { return !(*this < other); }
  };
  
  inline std::ostream &operator<<(
    std::ostream &out, const sa::Location &location
  )
  {
    return out << "@" << location.offset;
  }
}

template <> struct std::hash<sa::Location> {
  std::size_t operator()(const sa::Location &location) const
  {
    return hash<unsigned>()(location.offset);
  }
};

#endif
