// Copyright 2018-2023 Johan Cockx
#ifndef __base_ptrset_h
#define __base_ptrset_h

#include <set>

namespace base {

  // Set of pointers or pointer-like objects to comparable objects.  Comparison
  // is done on the objects, not the pointers. A ptrset will never contain
  // pointers to objects that compare equal, even if the pointers are different.
  // 
  template <class P>
  class ptrset {
    static bool compare(P item1, P item2)
    {
      return *item1 < *item2;
    }

    std::set<P, decltype(&compare)> set;

  public:
    ptrset(): set(compare) {}

    bool empty() const { return set.empty(); }

    void insert(P item)
    {
      set.insert(item);
    }

    void erase(P item)
    {
      auto it = set.find(item);
      if (*it == item) {
        set.erase(it);
      }
    }

    P find(P item, P notfound = 0) const
    {
      auto it = set.find(item);
      return it==set.end() ? notfound : *it;
    }
  
    P find_or_insert(P &&item)
    {
      auto it = set.find(item);
      if (it==set.end()) {
        insert(item);
        return item;
      } else {
        return *it;
      }
    }

    typename std::set<P, decltype(&compare)>::iterator begin()
    {
      return set.begin();
    }

    typename std::set<P, decltype(&compare)>::iterator end()
    {
      return set.end();
    }

    void swap(ptrset &other)
    {
      set.swap(other.set);
    }
  };
  
}
    
#endif
