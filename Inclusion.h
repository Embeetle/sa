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

#ifndef __Inclusion_h
#define __Inclusion_h

#include "Occurrence.h"

namespace sa {

  class Inclusion: public Occurrence
  {
  public:
    // The hdir used for this file inclusion occurrence.
    const base::ptr<Hdir> hdir;

    base::ptr<Hdir> get_hdir() const override;

    base::ptr<File> get_includee() const;

    base::ptr<File> get_includer() const
    {
      return file;
    }

    Inclusion(
      base::ptr<File> includee,
      base::ptr<File> includer,
      const Range &range,
      base::ptr<Hdir> hdir
    );
      
    ~Inclusion();

    // Aux method for operator<<
    void write(std::ostream &out) const override;
  };
}

#endif
