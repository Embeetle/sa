// Copyright 2018-2024 Johan Cockx
#include "ManagedEntity.h"

//------------------------------------------------------------------------------
// For background on atomics and memory order, see comments in base/RefCounted.h
//------------------------------------------------------------------------------

sa::ManagedEntity::~ManagedEntity()
{
  trace("destroy entity");
  assert(!is_known());
}

bool sa::ManagedEntity::is_known() const {
  return known_count.load(std::memory_order_relaxed);
}


void sa::ManagedEntity::increment_known()
{
  trace("increment known for " << get_entity_name());
  auto known = known_count.fetch_add(1, std::memory_order_relaxed);
  if (!known) {
    trace("mark known");
    increment_ref_count();
    assert(!get_user_data());
    create_user_data();
  }
}

void sa::ManagedEntity::decrement_known()
{
  trace("decrement known for " << get_entity_name());
  auto known = known_count.fetch_sub(1, std::memory_order_acq_rel);
  if (known == 1) {
    trace("mark unknown");
    delete_user_data();
    set_user_data(0);
    decrement_ref_count();
  }
}
