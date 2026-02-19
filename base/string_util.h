// Copyright 2018-2023 Johan Cockx
#ifndef __base_string_util_h
#define __base_string_util_h

#include <string>

namespace base {

  // Return true iff value begins with prefix
  inline bool begins_with(std::string const &value, std::string const &prefix)
  {
    return std::equal(prefix.begin(), prefix.end(), value.begin());
  }

  // Return true iff value ends with suffix
  inline bool ends_with(std::string const &value, std::string const &suffix)
  {
    return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
  }

  // Return true iff the suffix of value starting at offset begins with substr
  inline bool substr_is(
    std::string const &value, size_t offset, std::string const &substr
  )
  {
    return std::equal(substr.begin(), substr.end(), value.begin() + offset);
  }

  // Return true iff the suffix of value starting at offset equals substr
  inline bool substr_ends_with(
    std::string const &value, size_t offset, std::string const &substr
  )
  {
    return value.size() == offset + substr.size()
      && substr_is(value, offset, substr);
  }

  // Remove all occurrences of a char from a string
  inline void remove_char(std::string &value, char c)
  {
    size_t outpos = value.find_first_of(c);
    if (outpos != std::string::npos) {
      for (size_t i = outpos+1; i < value.length(); ++i) {
        if (value[i] != c) {
          value[outpos++] = value[i];
        }
      }
      value.resize(outpos);
    }
  }

  inline size_t common_prefix_length(const char *s1, const char *s2)
  {
    size_t n = 0;
    while (s1[n] && s1[n] == s2[n]) n += 1;
    return n;
  }

  inline void remove_prefix(std::string &value, std::string const &prefix)
  {
    if (begins_with(value, prefix)) {
      value.erase(0, prefix.size());
    }
  }

  inline void remove_suffix(std::string &value, std::string const &suffix)
  {
    if (ends_with(value, suffix)) {
      value.resize(value.size() - suffix.size());
    }
  }
}

#endif
