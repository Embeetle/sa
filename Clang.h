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

#ifndef __Clang_h
#define __Clang_h

#include "File.h"
#include "Analyzer.h"
#include "base/ptr.h"
#include <string>

namespace sa {
  class Clang: public Analyzer {
    base::ptr<File> source_file;
    const std::string resource_path;
    const std::string compiler;

  public:
    Clang(Unit *unit,
      const std::string &compiler,
      const std::string &resource_path
    );
    
    ~Clang();

    // Run Clang analysis. Return true on success and false on failure.
    // An analysis that finds errors in the source code is still successful.
    bool run(const std::string &flag_buffer) override;

    // Currently using a single section.  TODO: separate .text and .data
    // sections, subsections for --function-sections and --data-sections.
  };
}

#endif
