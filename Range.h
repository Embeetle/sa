// Copyright 2018-2024 Johan Cockx
#ifndef __sa_Range_h
#define __sa_Range_h

#include "Location.h"
#include "base/debug.h"

namespace sa {
  struct Range {

    // The location of the beginning of this range.
    unsigned begin;

    // The location just beyond the end of this range.
    unsigned end;

    // An empty range is a range with begin==end; it still represents a
    // location.  A void range is a range with begin > end; it represents no
    // range.

    // Construct a range containing everything
    Range(): begin(0), end(-1)
    {
    }

    Range(unsigned begin, unsigned end): begin(begin), end(end)
    {
    }

    bool is_void() const
    {
      return end < begin;
    }

    void set_void()
    {
      begin = 1;
      end = 0;
    }

    // A void range is also considered empty
    bool is_empty() const
    {
      return end <= begin;
    }

    // Size only makes sense for non-void ranges.
    unsigned size() const
    {
      return end - begin;
    }

    // Return true iff this range has at least one character in common with the
    // other range.
    bool overlaps(const Range &other) const
    {
      return (
        begin < other.end
        && other.begin < end
        && !is_empty()
        && !other.is_empty()
      );
    }

    // Return true iff this range includes the given range.
    bool includes(const Range &range) const
    {
      return begin <= range.begin && range.end <= end;
    }

    // Return true iff this range contains the given offset.
    bool contains(unsigned offset) const
    {
      return begin <= offset && offset < end;
    }

    void shift(int amount)
    {
      begin += amount;
      end += amount;
    }

    // Range is usable as key for std::set and std::map.
    bool operator<(const Range& other) const
    {
      return begin < other.begin ? true :
        other.begin < begin ? false :
        end < other.end;
    }
    bool operator>(const Range& other) const
    {
      return other < *this;
    }
      
    // Location is usable as key for std::unordered_set and std::unordered_map
    // A hash function is defined below, near the end of this file.
    bool operator==(const Range& other) const
    {
      return begin == other.begin && end == other.end;
    }
    bool operator!=(const Range& other) const
    {
      return ! (*this == other);
    }
  };

  inline std::ostream &operator<<(std::ostream &out, const sa::Range &range)
  {
    return out << range.begin << "-" << range.end;
  }
}

template <> struct std::hash<sa::Range> {
  std::size_t operator()(const sa::Range &range) const
  {
    return hash<unsigned>()(range.begin) ^ hash<unsigned>()(range.end) << 7;
  }
};

#endif
