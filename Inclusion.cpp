// Copyright 2018-2024 Johan Cockx
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

