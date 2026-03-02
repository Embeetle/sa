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

#include "Inclusion.h"
#include "File.h"
#include "Hdir.h"
#include "Project.h"

sa::Inclusion::Inclusion(
  base::ptr<File> includee,
  base::ptr<File> includer,
  const Range &range,
  base::ptr<Hdir> hdir
)
  : Occurrence(
    OccurrenceKind_include, OccurrenceStyle_unspecified,
    includee, includer, range
  )
  , hdir(hdir)
{
  
}
      
sa::Inclusion::~Inclusion()
{
  assert(base::is_valid_or_null(hdir));
  assert(file->project->is_locked());
  // Drop occurrence while it still has an hdir!
  file->drop_occurrence(this);
}

base::ptr<sa::Hdir> sa::Inclusion::get_hdir() const { return hdir; }

base::ptr<sa::File> sa::Inclusion::get_includee() const
{
  return entity.static_cast_to<File>();
}

void sa::Inclusion::write(std::ostream &out) const
{
  Occurrence::write(out);
  if (hdir) {
    out << " hdir=" << hdir->path;
  }
}

