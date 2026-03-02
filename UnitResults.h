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

#ifndef __UnitResults_h
#define __UnitResults_h

namespace sa {

  struct UnitResults {
    bool success;
    
    std::vector<base::ptr<Occurrence>> occurrences;

    std::vector<base::ptr<Diagnostic>> diagnostics;

    // A linker relevant error is an error that affects the occurrence of global
    // symbols in the compilation unit.  This flag is set if there is at least
    // one linker relevant error.
    bool has_linker_relevant_error = false;

    std::map<Occurrence*, sa::Scope> scope_data;

    std::vector<std::string> missing_headers;

    std::vector<base::ptr<Section>> sections;

    std::vector<base::ptr<EmptyLoop>> empty_loops;

    // Files used in this unit that contain at least one non UTF8 character.
    std::set<base::ptr<File>> non_utf8_files;

    bool from_cache;

    bool has_alternative_content;

    std::string alternative_content;
  };

}

#endif
