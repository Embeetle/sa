// Copyright © 2018-2026 Johan Cockx
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// SPDX-License-Identifier: GPL-3.0-or-later

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
