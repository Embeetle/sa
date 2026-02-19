// Copyright 2018-2024 Johan Cockx
#ifndef __Occurrence_h
#define __Occurrence_h

#include "Range.h"
#include "OccurrenceKind.h"
#include "OccurrenceStyle.h"
#include "Severity.h"
#include "base/ptr.h"
#include "base/RefCounted.h"
#include "base/debug.h"
#include <set>
#include <iostream>

namespace sa {
  class File;
  class Entity;
  class Hdir;
  class Section;
  class Diagnostic;
  
  class Occurrence: public base::RefCounted {
  
  public:
    // The entity (symbol or file) that occurs at this location
    const base::ptr<Entity> entity;

    // The file and location at which the entity occurs
    const base::ptr<File> file;

    const Range &get_range() const { return range; };

    // The kind of occurrence
    const OccurrenceKind kind;
    
    // The occurrence style (data, function, ...)
    const OccurrenceStyle style;
    
    ~Occurrence();

    // Do not call this constructor directly,  but use File::get_occurrence()!
    Occurrence(
      OccurrenceKind kind,
      OccurrenceStyle style,
      base::ptr<Entity> entity,
      base::ptr<File> file,
      const Range &range
    );

    // For an occurrence of kind "include", the hdir that was used if any;
    // otherwise null.
    virtual base::ptr<Hdir> get_hdir() const;

    // Return true iff this occurrence is instantiated in at least one
    // compilation unit. This returns false for occurences created during source
    // code analysis but not yet reported to the compilation unit. It is
    // possible that an occurrence is never instantiated, for example because
    // the source code analysis is cancelled before it can report its results.
    bool is_instantiated() const
    {
      return instance_count;
    }

    // Return true iff this occurrence is instantiated in at least one linked
    // compilation unit.
    bool is_linked() const
    {
      return linked_instance_count;
    }

    // Return true iff this occurrence can be a scope, i.e. can contain nested
    // occurrences,  based on the occurrence's kind and entity kind.
    bool can_be_scope() const;

    // Return true iff this is a defining occurrence: a strong, weak or
    // tentative definition.
    bool is_defining() const
    {
      return kind < NumberOfDefiningOccurrenceKinds;
    }

    // Return true iff this is a declaring occurrence: a definition or a
    // declaration.
    bool is_declaring() const
    {
      return kind < NumberOfDeclaringOccurrenceKinds;
    }

    // Get an existing or new diagnostic at the location of this occurrence with
    // the specified message and severity. Note that this does not display the
    // diagnostic in the application: all it does is to return a (possibly
    // shared) representation of the diagnostic.
    base::ptr<sa::Diagnostic> get_diagnostic(
      const std::string &message,
      Severity severity
    );

    // An occurrence in a header file will be instantiated in each compilation
    // unit that includes the header file. If a compilation unit includes the
    // same header file twice, it will be instantiated twice.
    //
    // An instance can be linked or not.  An instance is linked if it is part of
    // the application, i.e. included by the linker.
    
    // Insert an instance of this occurrence with the given linked status.
    void insert_instance(bool linked);

    // Remove an instance of this occurrence with the given linked status.
    void remove_instance(bool linked);

    // Aux method for operator<<
    virtual void write(std::ostream &out) const;

#ifndef NDEBUG
    std::string get_debug_name() const override;
#endif

  private:

    Range range;

    // Even though the range of an occurrence is normally constant, it can
    // change when the underlying file is edited. When that happens, it is the
    // responsibility of the File class to change its own data structures
    // accordingly.
    friend class File;

    // The number of instances of this occurrence in compilation units,
    // excluding instances in analyzers. The instance count controls
    // registration of the occurrence in its entity: it is registered iff the
    // instance count is non-zero.
    //
    // The instance count can be larger than the number of compilation units
    // because a compilation unit may hold more than one instance of the same
    // occurrence, for example if it includes the same file twice.
    size_t instance_count = 0;

    // The number of instances of this occurrence in linked compilation units
    // only.
    size_t linked_instance_count = 0;

  public:
    // Internal field that must be public because occurrences are inserted in a
    // MemberList, which is a set with constant time insertion and removal.
    //
    // Index in entity's list of occurrences of that entity. For an occurrence
    // of a file (a #include occurrence), this is the index of this occurrence
    // in the file's includers list. For an occurrence of a symbol, this is the
    // index of this occurrence in the symbol's occurrence list.
    size_t entity_index = 0;
  };

  std::ostream &operator<<(std::ostream &out, const Occurrence &occurrence);
}

#endif
