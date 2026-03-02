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

#ifndef __base_ptr_h
#define __base_ptr_h

#include "debug.h"

namespace base {

  // A smart pointer that manages the reference count of a reference counted
  // base object such as an object derived from RefCounted.
  //
  // The reference counted object must have the following methods:
  //   void increment_ref_count()
  //   void decrement_ref_count()
  //
  // To make sure that a statically constructed object is never deleted when
  // deleting the last smart pointer to it, constructors of the reference
  // counted object should initialize the reference count to one. Decrementing
  // the reference count to zero should destroy the object.
  //
  // These smart pointers are not thread safe: each smart pointer should be used
  // in one thread at a time. If the underlying object is thread-safe, however,
  // smart pointers to the same object in different threads can be used at the
  // same time without additional synchronization.
  //
  // This smart pointer implementation has some advantages over the standard
  // shared_ptr implementation:
  //
  //  1. Always a single allocation for base object and reference count together.
  //
  //  2. No risk of accidentally allocating two reference counts for the same
  //     object.
  //
  //  3. Smart pointers to statically allocated objects and objects on the stack
  //     are possible, without the risk that destruction of the last smart
  //     pointer invokes the object destructor.
  //
  //  4. Not exclusive: the reference count can also be manipulated in different
  //     ways.
  //
  // It also has some disadvantages:
  //
  //  1. No equivalent weak pointer implementation.  Plain pointers can be used
  //     instead to some extent, but there is no way to check that the
  //     underlying object still exists. The application must ensure that the
  //     underlying object cannot be destroyed while a smart pointer is
  //     constructed from a plain pointer.
  //
  //  2. Not compatible with objects of built-in types such as 'int' that do not
  //     have the required methods to manipulate the reference count.  Built-in
  //     types need to be wrapped in a class that includes a reference count.
  //     This can be implemented generically using a template if desired.

  template <class Base> class ptr {
  public:

    // Get a smart pointer to a new reference counted object, passing arguments
    // to the object's constructor. 
    template <class... Args>
    static ptr create(Args&&... args)
    {
      trace_nest("ptr create");
      return ptr(new Base(args...), true);
    }

    // Default constructor with null pointer.
    ptr(): base(0)
    {
      trace(this << " ptr default constructor");
    }
    
    // Constructor from plain pointer to existing reference counted object.
    //
    // Careful: use this only if the object is guaranteed to have a non-zero
    // reference count. This can be tricky especially in a multi-threaded
    // environment.
    //
    // Plain pointers have the same role as weak pointers in the standard
    // library, except that there is no way to check that the underlying object
    // still exists. Application logic at a higher level must ensure that the
    // underlying object is not deleted before or while this constructor runs.
    ptr(Base *base): base(base)
    {
      trace(this << " ptr from plain pointer " << base);
      increment_base_ref_count();
    }
    
    // Copy constructor.
    ptr(const ptr &other): base(other.base)
    {
      trace_nest(this << " ptr copy constructor from " << &other);
      increment_base_ref_count();
    }

    // Move constructor. Sets the other ptr to null.
    ptr(ptr &&other): base(other.base)
    {
      trace(this << " ptr move constructor from " << &other);
      assert_(is_valid_or_null(base), (void*)base);
      other.base = 0;
    }

    // Implicit cast constructor. Construct a ptr from a ptr of which the
    // underlying pointer type can be implicitly converted to the new ptr's
    // pointer type.
    template <class Other>
    ptr(ptr<Other> const &other): base((Base*)other)
    {
      trace(this << " ptr implicit conversion from " << &other);
      increment_base_ref_count();
    }

#if 0
    // Cast operator, to convert to a compatible type without creating a new ptr
    // instance.  TODO: this is only valid if no pointer offset.
    template <class Other>
    operator ptr<Other> &()
    {
      trace(this << " cast");
      Other *x = base; (void)x; // Verify pointer compatibility
      return *(ptr<Other>*)(this);
    }

    // Const cast operator, to convert to a compatible type without creating a
    // new ptr instance.  TODO: this is only valid if no pointer offset.
    template <class Other>
    operator ptr<Other> const&() const
    {
      trace(this << " const cast");
      Other *x = base; (void)x; // Verify pointer compatibility
      return *(ptr<Other> const*)(this);
    }
#endif

    // Destructor. The base object will self-destroy if this is the last smart
    // pointer.
    ~ptr()
    {
      trace_nest(this << " ptr destructor");
      decrement_base_ref_count();
    }
    
    // Assignment.
    ptr &operator=(const ptr &other)
    {
      trace_nest(this << " ptr assignment from " << &other);
      other.increment_base_ref_count();
      decrement_base_ref_count();
      base = other.base;
      return *this;
    }

    // Move-assignment.
    ptr &operator=(ptr &&other)
    {
      trace(this << " ptr move assignment from " << &other);
      decrement_base_ref_count();
      base = other.base;
      assert_(is_valid_or_null(base), (void*)base);
      other.base = 0;
      return *this;
    }

    // Dereference.
    Base &operator*() const
    {
      //trace(this << " ptr deref");
      assert_(is_valid(base), (void*)base);
      return *base;
    }

    // Arrow
    Base *operator->() const
    {
      //trace(this << " ptr arrow");
      assert_(is_valid(base), (void*)base);
      return base;
    }

    // Convert to underlying pointer type.
    operator Base*() const
    {
      //trace(this << " ptr to pointer");
      assert_(is_valid_or_null(base), (void*)base);
      return base;
    }

    // Statically cast the underlying pointer to the Other type and return a new
    // ptr.
    template <class Other>
    ptr<Other> static_cast_to() const
    {
      trace_nest(this << " ptr static cast");
      return ptr<Other>(static_cast<Other*>(base));
    }

    // Dynamically cast the underlying pointer to the Other type and return a new
    // ptr.
    template <class Other>
    ptr<Other> dynamic_cast_to() const
    {
      trace_nest(this << " ptr dynamic cast");
      return ptr<Other>(dynamic_cast<Other*>(base));
    }

    // Get the current reference count of the underlying object if not null.
    //
    // Careful: in a multithreaded environment, the reference count can change
    // at any time due to actions of other threads.
    size_t get_ref_count() const
    {
      //trace(this << " ptr get ref count");
      assert_(is_valid(base), (void*)base);
      return base->get_ref_count();
    }
    
  protected:

    // Constructor for newly allocated reference counted object.  Since the
    // initial reference count is already one, this does not increment the
    // reference count. The bool parameter is not used: it is only present to
    // make the signature of this constructor different from the signature of
    // the constructor from a pointer to an existing object.
    ptr(Base *base, bool): base(base)
    {
      trace(this << " ptr for new object (ref cnt = 1)");
      assert_(is_valid(base), (void*)base);
    }

    void increment_base_ref_count() const
    {
      if (base) {
        assert_(is_valid(base), (void*)base);
        trace(this << " ptr inc ref cnt of id=" << base->get_debug_id()
          << " to " << (base->get_ref_count()+1)
        );
        base->increment_ref_count();
      }
    }

    void decrement_base_ref_count() const
    {
      if (base) {
        assert_(is_valid(base), (void*)base);
        trace(this << " ptr dec ref cnt of id=" << base->get_debug_id()
          << " to " << (base->get_ref_count()-1)
        );
        base->decrement_ref_count();
      }
    }

  private:
    Base *base;
  };

  template<class Base>
  inline bool is_valid(ptr<Base> p)
  {
    return is_valid(static_cast<Base*>(p));
  }

  template<class Base>
  inline bool is_valid_or_null(ptr<Base> p)
  {
    return is_valid_or_null(static_cast<Base*>(p));
  }
}

#endif
