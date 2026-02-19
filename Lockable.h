// Copyright 2018-2024 Johan Cockx
#ifndef __Lockable_h
#define __Lockable_h

#include <pthread.h>

// A Lockable object is an object with a mutex.
class Lockable {

  // The mutex.
  mutable pthread_mutex_t mutex;

public:
  // Create an unlocked lockable object.
  Lockable() { pthread_mutex_init(&mutex, 0); }

  // No copying.
  Lockable(const Lockable&) = delete;  

  // Destroy.
  virtual ~Lockable() { pthread_mutex_destroy(&mutex); }

  // Lock the mutex.
  void lock() const { pthread_mutex_lock(&mutex); }

  // Unlock the mutex.
  void unlock() const { pthread_mutex_unlock(&mutex); }

  // Return true iff the mutex is locked. The result is only reliable if the
  // mutex is locked by the current thread. Otherwise, the lock status might
  // change before this function returns.
  bool is_locked() const
  {
    bool locked = pthread_mutex_trylock(&mutex);
    if (!locked) {
      pthread_mutex_unlock(&mutex);
    }
    return locked;
  }

  class Lock {
  public:
    Lock(const Lockable *lockable): lockable(lockable) { lockable->lock(); }
    Lock(const Lock&) = delete;
    ~Lock() { lockable->unlock(); }
  private:
    const Lockable *lockable;
  };
};

#endif
