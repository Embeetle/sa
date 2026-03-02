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

#ifndef __sa_Entity_h
#define __sa_Entity_h

#include "EntityKind.h"
#include "OccurrenceKind.h"
#include "base/RefCounted.h"
#include "base/ptr.h"
#include "base/debug.h"

namespace sa {
  class Project;
  class Occurrence;
  class GlobalSymbol;
  class LocalSymbol;
  class Symbol;
  class File;

  // An entity is something that can occur in source code: either a symbol or a
  // source file (in an #include statement). An entity has a name, kind and
  // project and tracks its occurrences.
  //
  // An entity can be known to the application, meaning that the application can
  // hold a handle for the entity and use that in API calls.
  //
  // To avoid dangling pointers when an entity is removed from the source code
  // by a background thread while the application holds a handle for it, there
  // is a "known" counter that can be incremented and decremented. As long as
  // the "known" counter is non-zero, the entity will not be deleted, even if it
  // no longer occurs in the source code.
  //
  // An entity has user data to be set and used by the application.  User data
  // is a void pointer that only has meaning to the user application; it can be
  // for example the address of an object representing the entity in the user
  // application.
  //
  // By default, is_known() returns true as long as the entity lives. Derived
  // classes can choose a different strategy: see for example ManagedEntity, for
  // which is_known() is only true when the known counter is non-zero, and for
  // which user data is automatically created when the entity becomes known and
  // deleted when it becomes unknown.
  //
  // Note on alternative strategy to avoid dangling pointers (not used)
  // ------------------------------------------------------------------
  //
  // Instead of keeping the entity alive to avoid a dangling pointer, it is
  // tempting to notify the application with a callback when an entity is
  // removed. This callback removes application data for the entity.
  //
  // This alternative approach is less attractive because it requires more
  // locking, including explicit lock management from the application.
  //
  // It is essential that the entity cannot be removed while the application is
  // making a call that uses the entity, because the application cannot cancel
  // the call. This requires a mutex on the project that can be locked and
  // unlocked from the application.  The locking and unlocking cannot be done in
  // the source analyzer, because once execution reaches source analyzer code,
  // it is too late to cancel the call.  The mutex cannot be on the entity,
  // because that requires using the entity handle in the lock call, and the
  // application cannot use the entity handle until the mutex is locked; this is
  // a chicken-and-egg problem. The callback to notify the application that an
  // entity is removed must lock the same mutex while it calls the callback, and
  // the entity must remain valid until the mutex is locked.
  // 
  class Entity: public base::RefCounted {
  public:
    EntityKind const kind;
    Project * const project;

    // Constructor. Entity is initially unknown with null user data.
    Entity(EntityKind kind, Project *project);

    // No copying
    Entity(const Entity&) = delete;

    // Return true iff entity is a symbol
    bool is_symbol() const { return is_symbol_kind(kind); }

    // Return true iff entity is a global symbol
    bool is_global_symbol() const { return is_global_symbol_kind(kind); }

    // Return true iff entity is a local symbol
    bool is_local_symbol() const { return is_local_symbol_kind(kind); }

    bool is_file() const { return kind == EntityKind_file; }
    
    bool is_local_function() const { return kind == EntityKind_local_function; }

    // Return this entity as symbol or assert.
    virtual Symbol *as_symbol();

    // Return this entity as global symbol or assert.
    virtual GlobalSymbol *as_global_symbol();

    // Return this entity as local symbol or assert.
    virtual LocalSymbol *as_local_symbol();

    // Return this entity as file or assert.
    virtual File *as_file();

    // Return the effective entity kind. For global symbols, this may differ
    // from the stored kind.
    virtual EntityKind get_effective_kind() const { return kind; }

    // Get a user-readable name for this entity.
    std::string get_name() const { return get_entity_name(); }
    virtual std::string get_entity_name() const { return ""; }

    // Increment the "known" counter. The entity is kept alive as long as the
    // known counter is non-zero. 
    virtual void increment_known();

    // Decrement the "known" counter. The entity is kept alive as long as the
    // known counter is non-zero. 
    virtual void decrement_known();

    // True iff this entity is known by the user application.
    virtual bool is_known() const { return true; }
    
    // Get user data
    void *get_user_data() const { return user_data; }

    // Set user data
    void set_user_data(void *data) { user_data = data; }

    // Get scope of first occurrence if this is a local symbol,  otherwise null.
    virtual Occurrence *get_ref_scope() const { return 0; }

    // Return symbol if this is a symbol, otherwise null.
    Symbol *get_symbol() { return is_symbol() ? as_symbol() : 0; }
  
    // Return file if this is a file, otherwise null.
    File *get_file() { return is_file() ? as_file() : 0; }

    // Track occurrences of this entity. Only occurrences whose kind is in the
    // occurrence kind set *and* whose inclusion status is in the inclusion
    // status set will be tracked. Initially, both sets are empty.  To stop
    // tracking, make at least one of them empty again.
    //
    // Occurrences are tracked by calling the project's add_occurrence_of_entity
    // and remove_occurrence_of_entity callbacks. These callbacks must be valid
    // functions unless one of the sets is empty.
    //
    // When the occurrences to be tracked change,  the appropriate callbacks
    // are immediately executed to add or remove occurrences.
    virtual void track_occurrences_of_entity(OccurrenceKindSet)
    {
    };

    // Virtual functions insert_instance and remove_instance allow derived
    // entities (symbols and files) to keep track of all occurrences of the
    // entity. Each occurrence is inserted once per instantiation in a
    // compilation unit. A file can be included in more than one compilation
    // unit and can even be included more than once in the same compilation
    // unit, so there can be more than one instantiation of the same
    // occurrence.
    //
    // Instances have a link status, indicating for each instance whether it is
    // in a linked compilation unit.  When the link status of a file changes,
    // instances are first inserted with the new status and then removed with
    // the old status. This avoids removing and then re-inserting occurrences
    // for some implementations.  One consequence is that the link status of the
    // file cannot be determined directly from the file.  It is instead passed
    // as an argument to insert_instance and remove_instance.
    //
    // These functions can be used to count the number of instances of a symbol
    // or file, per link status if desired. This is useful, for example, to
    // determine whether a symbol is multiply-defined.  The number of linked
    // instances of a file is useful, for example, to determine the inclusion
    // status of a file.
    //
    // The insert function has boolean parameters indicating whether the
    // inserted instance is the first instance or the first instance in a linked
    // file.  Similarly, the remove function has boolean parameters indicating
    // whether the removed instance is the last instance or the last instance in
    // a linked file. This allows an entity to keep track of all its occurrences
    // in the source code, without having to check for duplicates. This is
    // required for cross-referencing of symbols and files.
    //
    // The cost of these boolean parameters is an instance counter and a
    // linked instance counter in each occurrence object. This is a trade-off
    // between memory usage and speed: counters may take more memory - because
    // there are a lot of occurrences - but they are faster than checking a set
    // of occurrences for duplicates.
    //
    // A null occurrence pointer represents the implicit instantiation of the
    // compilation unit's main file. When the occurrence pointer is null, the
    // boolean parameters are always false. For symbols, the occurrence pointer
    // is never null.
     
    // Insert an instance of an occurrence of this entity in a compilation unit
    // with the given link status. Called when the occurrence is instantiated
    // and when the link status changes.
    //
    // The 'first' flag is true when the first instance of the occurrence is
    // inserted.  The 'first_linked' flag is true when the first instance of
    // the occurrence in a linked compilation unit is inserted.
    virtual void insert_instance_of_entity(
      bool linked,
      Occurrence *occurrence,
      bool is_first_instance,
      bool is_first_linked_instance
    )
    {
      (void)linked;
      (void)occurrence;
      (void)is_first_instance;
      (void)is_first_linked_instance;
    }
    
    // Remove an instance of an occurrence of this entity in a compilation unit
    // with the given link status. Called when an instantiation of the
    // occurrence is removed and when the link status changes.
    //
    // The 'last' flag is true when the last instance of the occurrence is
    // removed.  The 'last_linked' flag is true when the last instance of the
    // occurrence in a linked compilation unit is removed.
    virtual void remove_instance_of_entity(
      bool linked,
      Occurrence *occurrence,
      bool is_last_instance,
      bool is_last_linked_instance
    )
    {
      (void)linked;
      (void)occurrence;
      (void)is_last_instance;
      (void)is_last_linked_instance;
    }

#ifndef NDEBUG
    std::string get_debug_name() const { return get_entity_name(); }
#endif

  protected:
    
    ~Entity();

    void *user_data = 0;
    
  private:
    // The number of registered instances of occurrences.
    size_t instance_count = 0;

    // The "known" counter.
    //
    // A separate known counter in addition to the reference counter is not
    // required to keep the entity alive: that can be achieved by incrementing
    // the reference counter. It is however required to manage user data,
    // i.e. create it when the entity becomes known and delete it when it
    // becomes unknown.
    std::atomic_uint known_count = 0;
  };

  std::ostream &operator<<(std::ostream &out, const Entity &entity);
}

#endif
