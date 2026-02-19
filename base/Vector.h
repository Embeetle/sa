// Copyright 2018-2023 Johan Cockx
#ifndef __Vector_h
#define __Vector_h

#include "debug.h"
#include <vector>
#include <algorithm>
#include <iostream>

//--------------------------------------------------------------------------
template <typename T> class Vector: public std::vector<T> {
public:
  typedef typename std::vector<T>::reference reference;
  typedef typename std::vector<T>::const_reference const_reference;
  typedef typename std::vector<T>::size_type size_type;
  typedef typename std::vector<T>::const_iterator const_iterator;

  const_iterator begin() const { return std::vector<T>::begin(); }
  const_iterator end() const { return std::vector<T>::end(); }

  // Create an empty vector.
  Vector() {}

  // Create a vector with one element.
  explicit Vector(const_reference t) { push_back(t); }

  // Create a vector with two elements.
  explicit Vector(const_reference t1, const_reference t2) {
    push_back(t1);
    push_back(t2);
  }

  // Create a vector with three elements.
  explicit Vector(const_reference t1, const_reference t2, const_reference t3)
  {
    push_back(t1);
    push_back(t2);
    push_back(t3);
  }

  // Create a vector with four elements.
  explicit Vector(
    const_reference t1,
    const_reference t2,
    const_reference t3,
    const_reference t4
  )
  {
    push_back(t1);
    push_back(t2);
    push_back(t3);
    push_back(t4);
  }

  // Create a vector with five elements.
  explicit Vector(
    const_reference t1,
    const_reference t2,
    const_reference t3,
    const_reference t4,
    const_reference t5
  )
  {
    push_back(t1);
    push_back(t2);
    push_back(t3);
    push_back(t4);
    push_back(t5);
  }

  // Create a vector with six elements.
  explicit Vector(
    const_reference t1,
    const_reference t2,
    const_reference t3,
    const_reference t4,
    const_reference t5,
    const_reference t6
  )
  {
    push_back(t1);
    push_back(t2);
    push_back(t3);
    push_back(t4);
    push_back(t5);
    push_back(t6);
  }

  // Create a vector with seven elements.
  explicit Vector(
    const_reference t1,
    const_reference t2,
    const_reference t3,
    const_reference t4,
    const_reference t5,
    const_reference t6,
    const_reference t7
  )
  {
    push_back(t1);
    push_back(t2);
    push_back(t3);
    push_back(t4);
    push_back(t5);
    push_back(t6);
    push_back(t7);
  }

  class Size {
  public:
    Size(size_type size): _size(size) {}
    operator size_type() const { return _size; }
  protected:
    size_type _size;
  };

#ifdef DEFINE_VECTOR_CONSTRUCTOR_WITH_GIVEN_NR_OF_DEFAULT_VALUES
  // Create a vector with n elements,  default initialized.
  // To avoid ambiguitiy for Vector<size_type>,  argument type is Size
  explicit Vector(const Size &n): std::vector<T>(n) {}
#endif

  // Ensure that the capacity is at least n elements.
  void reserve(size_type n) { std::vector<T>::reserve(n); }

  // Create a vector with n elements with specified value.
  // To avoid ambiguitiy for Vector<size_type>,  argument type is Size
  explicit Vector(const Size &n, const_reference t): std::vector<T>(n,t) {}

  // Create a copy of a vector.
  Vector(const Vector<T>& v): std::vector<T>(v) {}

  // Create a vector with n elements with specified values.
  Vector(const T* v, size_type n): std::vector<T>(n) {
    for (size_type i = n; i--; ) {
      set_at(i,v[i]);
    }
  }

  // The empty vector.
  static const Vector& none() { return _none; }

  // Return true if vector is empty.
  bool empty() const { return std::vector<T>::empty(); }

  // Return the number of elements in the vector.
  size_type size() const { return std::vector<T>::size(); }

  // Return a read-write reference to the i'th element.
  reference at(size_type i) {
    assert(i < size());
    return (*(std::vector<T>*)this)[i];
  }
  reference operator[](size_type i) { return at(i); }

  // Return a read-only reference to the i'th element.
  const_reference at(size_type i) const {
    assert(i < size());
    return (*(const std::vector<T>*)this)[i];
  }
  const_reference operator[](size_type i) const { return at(i); }

  // Return a read-only reference to the first element.
  const_reference front() const { return std::vector<T>::front(); }

  // Return a read-write reference to the first element.
  reference front() { return std::vector<T>::front(); }

  // Return a read-only reference to the last element.
  const_reference back() const { return std::vector<T>::back(); }

  // Return a read-write reference to the last element.
  reference back() { return std::vector<T>::back(); }

  // Test vector equality.  Vectors are equal if elements are equal one by
  // one.
  friend bool operator==(const Vector& v1, const Vector& v2) {
    return (const std::vector<T>&)v1 == (const std::vector<T>&)v2;
  }
    
  // True if vector contains element at least once.
  bool includes(const_reference t) const {
    return std::find(std::vector<T>::begin(),std::vector<T>::end(),t)
      != std::vector<T>::end();
  }

  // True if vector does not contain element.
  bool excludes(const_reference t) const { return !includes(t); }

  // True if vector contains all elements of another vector at least once.
  bool includes(const Vector& vector) const {
    for (size_type i = vector.size(); i--; ) {
      if (!includes(vector.at(i))) {
        return false;
      }
    }
    return true;
  }

  // True if vector does not contain any elements of another vector.
  bool excludes(const Vector<T>& vector) const {
    for (size_type i = vector.size(); i--; ) {
      if (includes(vector.at(i))) {
        return false;
      }
    }
    return true;
  }

  // Remove all elements.
  void clear() { std::vector<T>::clear(); }

  // Add an element at the back.
  void push_back(const_reference t) { std::vector<T>::push_back(t); }

  // Add an element at the front;  existing elements move to next position.
  void push_front(const_reference t) { insert_at(0,t); }

  // Remove the last element.
  void pop_back() { std::vector<T>::pop_back(); }

  // Remove a consecutive range of elements
  void erase_range(size_type head, size_type tail) {
    std::vector<T>::erase(
      std::vector<T>::begin()+head,std::vector<T>::begin()+tail
    );
  }

  // Set the i'th element.
  void set_at(size_type i, const_reference t) {
    assert(i < size());
    (*this)[i] = t;
  }

  // Insert an element at a specific position.  Existing elements at or beyond
  // that position move to the next position.
  void insert_at(size_type i, const_reference t) {
    this->resize(size()+1);
    for (size_type j = size(); --j > i; ) {
      set_at(j,at(j-1));
    }
    set_at(i,t);
  }

  // Remove an element at a specific position.  Existing elements beyond that
  // position move to the previous position to fill the gap.
  void erase_at(size_type i) {
    while (i < size()-1) {
      set_at(i,at(i+1));
      ++i;
    }
    pop_back();
  }

  // Insert a new element to a vector representing a set.
  void add(const_reference t) {
    //assert(!includes(t));
    push_back(t);
  }

  // Insert a new element in a vector representing a set.
  void insert(const_reference t) {
    //assert(!includes(t));
    push_back(t);
  }

  // Remove an element from a vector representing a set. Assert that the
  // vector contained the element.
  void remove(const_reference t) {
    //assert(includes(t));
    erase(find(std::vector<T>::begin(),std::vector<T>::end(),t));
    //assert(!includes(t));
  }

  // Remove an element at a specific position in a vector representing a set.
  // Constant time:  the removed element is replaced by the last element.
  void remove_at(size_t i) {
    set_at(i,back());
    pop_back();
  }

  // Add an element to a vector representing a set, if the vector did not
  // contain the element yet.
  void include(const_reference t) {
    if (!includes(t)) {
      push_back(t);
    }
  }

  // Remove all instances of an element in a vector.
  void exclude(const_reference t) {
    if (includes(t)) {
      erase(find(std::vector<T>::begin(),std::vector<T>::end(),t));
    }
  }

  // Add elements of a vector to a vector representing a set. Assert that the
  // target vector did not contain the elements yet.
  void insert(const Vector& vector) {
    for (size_type i = vector.size(); i--; ) {
      insert(vector.at(i));
    }
  }
    
  // Remove elements of a vector from a vector representing a set. Assert that
  // the target vector did contain each element.
  void remove(const Vector& vector) {
    for (size_type i = vector.size(); i--; ) {
      remove(vector.at(i));
    }
  }

  void include(const Vector& vector) {
    for (size_type i = vector.size(); i--; ) {
      include(vector.at(i));
    }
  }
    
  void exclude(const Vector& vector) {
    for (size_type i = vector.size(); i--; ) {
      exclude(vector.at(i));
    }
  }

  void push_back(const Vector& vector) {
    for (size_type i = 0; i < vector.size(); i++ ) {
      push_back(vector.at(i));
    }
  }
    
  // Remove the last entry equal to a given value.  It is essential that the
  // last entry is removed, because when iterating over a changing container,
  // I usually iterate backwards to avoid problems; this trick only works if
  // changes only occur near the end of the container, at or after the current
  // position.
  void remove_last(const_reference t) {
    //
    // erase(find(rbegin(),rend(),t)); compilation error? Foert!
    for (size_type i = size(); i--; ) {
      if (at(i) == t) {
        while (i < size()-1) {
          (*this)[i] = (*this)[i+1];
          ++i;
        }
        std::vector<T>::pop_back();
        return;
      }
    }
    assert(false); // Attempt to remove non-existing element
  }
      
  //
private:
  static const Vector _none;
};
  
template <typename T> const Vector<T> Vector<T>::_none;

template <typename T>
std::ostream& operator<<(std::ostream& out, const Vector<T>& vector) {
  out << "{";
  for (typename Vector<T>::size_type i = 0; i < vector.size(); ++i) {
    if (i != 0) out << ",";
    out << vector.at(i);
  }
  out << "}";
  return out;
}
template <typename T>
Vector<T> operator+(const Vector<T>& vector1, const Vector<T>& vector2) {
  Vector<T> vector = vector1;
  vector.include(vector2);
  return vector;
}

#endif

