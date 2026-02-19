// Copyright 2018-2023 Johan Cockx
#ifndef __MemberList_h
#define __MemberList_h

#include "Vector.h"

//--------------------------------------------------------------------------
// A member list is a list of pointers to member object, where the member
// objects have a field containing their index in the member list.
//
// Member lists support amortized constant time addition and removal of members.
//
// To iterate over members, use an index.  If the current member can be removed
// during the iteration, use downward iteration: members at lower indices are
// not affected.
//
template <class Member, size_t Member::*m_index>
class MemberList {
public:
  typedef typename Vector<Member*>::size_type size_type;
  typedef typename Vector<Member*>::const_reverse_iterator const_iterator;

  const_iterator begin() const { return vector.rbegin(); }
  const_iterator end() const { return vector.rend(); }

  // Create an empty list.
  MemberList() {}

  // Destroy a list. Delete all members.
  ~MemberList() { clear(); }

  // Ensure that the capacity is at least n elements.
  void reserve(size_type n) { vector.reserve(n); }

  // Forbid copying
  MemberList(const MemberList<Member, m_index>& v) = delete;

  // Return true if list is empty.
  bool empty() const { return vector.empty(); }

  // Return the number of members in the list.
  size_type size() const { return vector.size(); }

  // Return a pointer to the i'th member.
  Member* at(size_type i) const { return vector.at(i); }

  // Return a vector with pointers to all members.
  const Vector<Member*> &members() const { return vector; }

  // Insert a new member in the list
  void insert(Member* member)
  {
    member->*m_index = size();
    vector.push_back(member);
  }

  // Remove an existing member from the list.  Replace it by the last member to
  // achieve constant time removal.
  void remove(Member* member)
  {
    size_type index = member->*m_index;
    size_type last_index = vector.size()-1;
    if (index != last_index) {
      Member *last_member = vector.at(last_index);
      last_member->*m_index = index;
      vector.set_at(index, last_member);
    }
    vector.pop_back();
  }

  // Check if a member is in this list, in constant time.
  bool includes(Member *member)
  {
    size_type index = member->*m_index;
    return index < vector.size() && vector.at(index) == member;
  }

  // Remove and return any member of a non-empty list.
  Member *pop()
  {
    size_type last_index = vector.size()-1;
    Member *member = vector.at(last_index);
    vector.pop_back();
    return member;
  }

  // Delete all members.  Member destructor should remove member from the list.
  void clear()
  {
    while (!empty()) {
      delete vector.back();
    }
  }

private:
  Vector<Member*> vector;
};
  
#endif

