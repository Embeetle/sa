// Copyright 2018-2024 Johan Cockx
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
