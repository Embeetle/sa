// Copyright 2018-2024 Johan Cockx
#ifndef __Command_h
#define __Command_h

#include <string>
#include <map>
#include <vector>
#include <iostream>

namespace sa {
  struct Command {
    std::vector<std::string> args;
    std::string working_directory;

    Command(
      std::vector<std::string> args,
      std::string working_directory
    )
    {
      args.swap(this->args);
      working_directory.swap(this->working_directory);
    }

    Command(std::string arg0)
      : working_directory(".")
    {
      args.push_back(arg0);
    }
  };

  inline std::ostream &operator<<(std::ostream &out, const Command &command)
  {
    bool first = true;
    for (auto arg: command.args) {
      if (first) {
        first = false;
      } else {
        out << " ";
      }
      out << arg;
    }
    return out;
  }
}

#endif
