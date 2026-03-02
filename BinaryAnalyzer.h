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

#ifndef __BinaryAnalyzer_h
#define __BinaryAnalyzer_h

#include "Analyzer.h"

namespace sa {
  class BinaryAnalyzer: public Analyzer {
  public:
    BinaryAnalyzer(Unit *unit);

    ~BinaryAnalyzer();

    // Run analysis of binary file: object file, archive, ... . Return true on
    // success and false on failure.  An analysis that finds errors is still
    // successful.
    bool run(const std::string &flag_buffer) override;

  protected:
    void handle_stdout(std::istream &in);
    void handle_stderr(std::istream &in);

  private:
    base::ptr<File> source_file;
  };
}

#endif

