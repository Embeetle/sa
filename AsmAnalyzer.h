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

#ifndef __AsmAnalyzer_h
#define __AsmAnalyzer_h

#include "Analyzer.h"
#include "ExternalAnalyzer.h"

namespace sa {
  class AsmAnalyzer: public ExternalAnalyzer {

  public:
    AsmAnalyzer(Unit *unit);

    ~AsmAnalyzer();

    // Run asm analysis. Return true on success and false on failure.
    // An analysis that finds errors in the source code is still successful.
    bool run(const std::string &flag_buffer) override;

  protected:
    void handle_stderr(std::istream &in) override;
    void handle_asm_error_line(const std::string &line);

    bool is_linker_relevant_diagnostic(
      sa::Severity severity,
      const std::string &message
    ) override;
    
  };
}

#endif
