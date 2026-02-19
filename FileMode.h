// Copyright 2018-2024 Johan Cockx
#ifndef __FileMode_h
#define __FileMode_h

namespace sa {
  // Mode enum for source files.
  enum FileMode {
    // A source file in excluded mode is never included in the project's
    // executable when building the project; that is, unless it is included in
    // another source file, e.g. by the preprocessor.
    FileMode_exclude,

    // A source file in included mode is always included in the project's
    // executable when building the project.
    FileMode_include,

    // For a source file in automatic mode, the source analyzer will determine
    // automatically whether it should be included or not in the project's
    // executable when building the project.
    FileMode_automatic,
  };
  static const char * const FileMode_names[] = {
    "exclude", "include", "automatic"
  };

  inline std::ostream &operator<<(std::ostream &out, FileMode mode)
  {
    return out << FileMode_names[mode];
  }
};

#endif
