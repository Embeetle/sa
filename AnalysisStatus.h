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

#ifndef __AnalysisStatus_h
#define __AnalysisStatus_h

#include <iostream>

namespace sa {

  // Analysis status for source files.
  enum AnalysisStatus {
    // Analysis is not required
    AnalysisStatus_none,

    // Analysis is scheduled
    AnalysisStatus_waiting,
  
    // Analysis is in progress
    AnalysisStatus_busy,
  
    // Analysis is done
    AnalysisStatus_done,
  
    // Analysis failed: file is unreadable or does not exist, analysis crashed,
    // flag extraction failed. Note: an analysis that detects errors did not
    // fail; it only fails when it cannot analyze the source files due to one of
    // the above reasons.
    AnalysisStatus_failed,
  };
  static const char * const AnalysisStatus_names[] = {
    "none", "waiting", "busy", "done", "failed"
  };

  inline std::ostream &operator<<(std::ostream &out, AnalysisStatus status)
  {
    return out << AnalysisStatus_names[status];
  }
}

#endif
