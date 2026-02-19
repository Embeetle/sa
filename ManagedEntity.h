// Copyright 2018-2024 Johan Cockx
#ifndef __sa_ManagedEntity_h
#define __sa_ManagedEntity_h

#include "Entity.h"
#include "base/debug.h"
#include <atomic>

namespace sa {
  class Project;

  // A managed entity is an entity that automatically manages user data.  More
  // specifically, it calls a virtual method to create user data when the entity
  // becomes known and calls another virtual method to delete user data when the
  // entity becomes unknown.
  //
  // Managing user data in this way usually saves memory at the cost of deleting
  // and recreating user data for the same entity. It requires an additional
  // counter to implement.
  //
  class ManagedEntity: public Entity {
  public:

    // Inherit constructors
    using Entity::Entity;

    // Increment the "known" counter, calling create_user_data when incrementing
    // from zero.
    void increment_known() override;

    // Decrement the "known" counter, calling delete_user_data when decrementing
    // to zero.
    void decrement_known() override;

    // True iff this entity is known by the user application.
    bool is_known() const override;
    
    // Create user data for this entity. Called when the known count is
    // incremented from zero.
    virtual void create_user_data() { }

    // Delete user data for this entity. By default called when the known count
    // is decremented to zero.
    virtual void delete_user_data() { }

  protected:
    ~ManagedEntity();

  private:
    // The "known" counter.
    //
    // A separate known counter in addition to the reference counter is not
    // required to keep the entity alive: that can be achieved by incrementing
    // the reference counter. It is however required to manage user data,
    // i.e. create it when the entity becomes known and delete it when it
    // becomes unknown.
    //
    // An atomic counter allows for multi-threaded applications.
    std::atomic_uint known_count = 0;
  };
}

#endif
