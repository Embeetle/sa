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

#ifndef __base_RefCounted_h
#define __base_RefCounted_h
#define __base_RefCounted_USE_ATOMIC 1
#if __base_RefCounted_USE_ATOMIC
#include <atomic>
#else
#include <pthread.h>
#endif
#include "debug.h"
#include <set>

namespace base {

  // A reference counted object is intended as a base class for objects that
  // should be automatically deleted when they are no longer referenced.
  //
  // It contains a reference count, initialized to one at construction
  // time. There are thread-safe methods to increment or decrement the reference
  // count.  When it is decremented to zero, the object self-destructs by
  // calling its virtual destructor.
  //
  // It can be used in combination with smart pointers (see ptr) or other
  // mechanisms that automatically increment and decrement the reference count.
  //
  class RefCounted: public base::Checked {
    
#if __base_RefCounted_USE_ATOMIC
    // For background on using atomics, see
    // https://en.cppreference.com/w/cpp/atomic/memory_order and Herb Sutter's
    // talk https://channel9.msdn.com/Shows/Going+Deep
    // /Cpp-and-Beyond-2012-Herb-Sutter-atomic-Weapons-2-of-2, or the related
    // discussion for Boost in https://www.boost.org/doc/libs/1_57_0/doc/html
    // /atomic/usage_examples.html#boost_atomic.usage_examples.
    // example_reference_counters:
    //
    //> Increasing the reference counter can always be done with
    //> memory_order_relaxed: New references to an object can only be formed
    //> from an existing reference, and passing an existing reference from
    //> one thread to another must already provide any required synchronization.
    //>
    //> It is important to enforce any possible access to the object in one
    //> thread (through an existing reference) to happen before deleting the
    //> object in a different thread. This is achieved by a "release" operation
    //> after dropping a reference (any access to the object through this
    //> reference must obviously happened before), and an "acquire" operation
    //> before deleting the object.
    //>
    //> It would be possible to use memory_order_acq_rel for the fetch_sub
    //> operation, but this results in unneeded "acquire" operations when the
    //> reference counter does not yet reach zero and may impose a performance
    //> penalty.
    //
    // Conclusion: incrementing the reference count can have
    // memory_order_relaxed while decrementing it must have
    // memory_order_acq_rel, to make sure that any access to the object before a
    // decrement in one thread happens before the destructor in another thread.
    // The ordering is imposed by the release in the first thread and the
    // acquire in the second thread. As mentioned in the Boost documentation,
    // the acquire is only needed in the thread that actually executes the
    // destructor. The solution proposed by Herb Sutter always performs an
    // acquire.
    //
    // Synchronisation between non-destructor accesses are the responsibility of
    // the application.
#endif
    
  public:

    RefCounted()
    {
#if !__base_RefCounted_USE_ATOMIC
      pthread_mutex_init(&mutex, 0);
#endif
#ifdef CHECK
      all.insert(this);
#endif
    }

    // Destructor.
    virtual ~RefCounted()
    {
      // For heap objects, this destructor is called when the reference count is
      // decremented to zero.  If it is called directly because of a stack or
      // static object going out of scope, a reference count greater than one
      // means that dangling references will be left behind; this is not allowed
      // (asserted).
      assert_(ref_count <= 1, "ref_count=" << ref_count
        << " id=" << get_debug_id() << " " << get_debug_name()
      );
#ifdef CHECK
      all.erase(this);
#endif
#if !__base_RefCounted_USE_ATOMIC
      pthread_mutex_destroy(&mutex);
#endif
    }

    // No copying
    RefCounted(const RefCounted&) = delete;  

    // Method called when reference count reaches zero.  Default implementation
    // self-destructs. Derived classes may have reasons for other actions.
    virtual void zero_ref_count()
    {
      assert(get_ref_count() == 0);
      delete this;
    }
    
    // Get the current reference count.
    size_t get_ref_count() const
    {
#if __base_RefCounted_USE_ATOMIC
      return ref_count.load(std::memory_order_relaxed);
#else
      pthread_mutex_lock(&mutex);
      size_t count = ref_count;
      pthread_mutex_unlock(&mutex);
      return count;
#endif
    }

    // Increment the reference count. The new reference count will be greater
    // than zero, and the object will not self-destruct until the reference
    // count is decremented again.
    //
    // This operation will fail with an assertion if the old reference count is
    // zero. In a multithreaded environment, a zero reference count may mean
    // that another thread has invoked self-destruction, so that it is no longer
    // possible to avoid self-destruction. Incrementing the reference count at
    // that point could lead to a dangling pointer.
    void increment_ref_count()
    {
#if __base_RefCounted_USE_ATOMIC
      auto count = ref_count.fetch_add((uintptr_t)1, std::memory_order_relaxed);
#else
      pthread_mutex_lock(&mutex);
      auto count = ref_count;
      ref_count++;
      pthread_mutex_unlock(&mutex);
#endif
      trace("RefCounted increment " << count << " -> " << (count+1));
      assert_(count, "unsafe inc from 0 #" << get_debug_id());
      assert_(count + 1, "too many ptr's for #" << get_debug_id());
#ifdef CHECK
      notify_ref_count();
#endif
    }

    // Decrement the reference count.  If the new reference count is zero,
    // self-destruct.
    void decrement_ref_count()
    {
#if __base_RefCounted_USE_ATOMIC
      auto count = std::atomic_fetch_sub_explicit(&ref_count, (uintptr_t)1,
        std::memory_order_acq_rel
      );
#else
      pthread_mutex_lock(&mutex);
      auto count = ref_count;
      ref_count--;
      pthread_mutex_unlock(&mutex);
#endif
      trace("RefCounted decrement " << count << " -> " << (count-1));
      assert_(count, "reference count underflow for #"
        << get_debug_id() << " " << get_debug_name()
      );
#ifdef CHECK
      notify_ref_count();
#endif
      if (count == 1) {
        zero_ref_count();
      }
    }

#ifndef NDEBUG
    virtual size_t get_debug_id() const { return object_number(); }
    virtual std::string get_debug_name() const { return "<no-name>"; };
#endif
#ifdef CHECK
    // Virtual method called after every change of the reference count.
    virtual void notify_ref_count() const {}

    // Trace all live ref-counted objects (see RefCounted.cpp for format)
    static void report_all();
#endif

  private:

    // Initialize ref count to one, not zero, to avoid accidentally attempting
    // to destroy static or heap objects.
    //
    // If the ref count is initialized to one, the creation and destruction of
    // reference counting pointers will never cause the reference count to go to
    // zero.
    //
    // If the object is created on the heap through a reference counted pointer,
    // its reference count should be decremented to enable automatic
    // self-destruction. The ptr::create factory method does this automatically.
#if __base_RefCounted_USE_ATOMIC
    std::atomic_uintptr_t ref_count = 1;
#else
    uintptr_t ref_count = 1;
    mutable pthread_mutex_t mutex;
#endif
#ifdef CHECK
    // Set of all ref-counted objects
    static std::set<RefCounted*> all;
#endif
  };
}

#endif
