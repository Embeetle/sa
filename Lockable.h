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
