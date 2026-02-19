// Copyright 2018-2024 Johan Cockx
#ifndef __sa_FileLocation_h
#define __sa_FileLocation_h

#include "base/ptr.h"
#include "File.h"

namespace sa {
  
  struct FileLocation: public Location {

    base::ptr<File> file;

    FileLocation() {}
    
    FileLocation(base::ptr<File> file, unsigned offset)
      : Location(offset), file(file)
    {
    }

    FileLocation(
      base::ptr<File> file,
      const Location &location
    ): Location(location), file(file)
    {
    }

    // FileLocation is usable as key for std::set and std::map
    bool operator<(const FileLocation& other) const
    {
      return file != other.file ? file < other.file :
        (const Location&)*this < (const Location&)other;
    }
      
    // FileLocation is usable as key for std::unordered_set and
    // std::unordered_map A hash function is defined below, near the end of this
    // file.
    bool operator==(const FileLocation& other) const
    {
      return file == other.file
        && (const Location&)*this == (const Location&)other;
    }

    bool operator!=(const FileLocation& other) const { return !(*this == other);}
    bool operator>(const FileLocation& other) const { return other < *this; }
    bool operator<=(const FileLocation& other) const { return !(*this > other); }
    bool operator>=(const FileLocation& other) const { return !(*this < other); }
  };

  // Implementation in File.cpp
  std::ostream &operator<<(std::ostream &out, const sa::FileLocation &location);
}

template <> struct std::hash<sa::FileLocation> {
  std::size_t operator()(const sa::FileLocation &location) const
  {
    return std::hash<sa::File*>()((sa::File*)(location.file))
      ^ hash<sa::Location>()(location);
  }
};

#endif
