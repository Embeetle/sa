// Copyright 2018-2023 Johan Cockx

#ifdef CHECK
#include "RefCounted.h"
#include "platform.h"

std::set<base::RefCounted*> base::RefCounted::all;

void base::RefCounted::report_all()
{
  trace_nest("RefCounted objects alive now");
  for (auto ptr: all) {
    trace("alive: " << type_name(*ptr) << " "
      << ptr->get_debug_id() << " " << ptr->get_debug_name()
    );
  }
}
#endif

//==============================================================================
#ifdef SELFTEST

#include "RefCounted.h"
#include "ptr.h"
#include "debug.h"
#include <stdio.h>
#include <set>

using base::ptr;

class LeakCheck {
public:
  ~LeakCheck();
};
class A;
static std::set<A*> as;
LeakCheck leak_check;

class A: public base::RefCounted
{
public:
  typedef base::ptr<A> ptr;
  A(): id(++next_id)
  {
    test_out(id << " default constructor");
    ++count;
    as.insert(this);
  }
  ~A()
  {
    test_out(id << " destructor");
    --count;
    as.erase(this);
  }
  unsigned id;
  size_t get_debug_id() const override { return id; }
  
  static unsigned next_id;
  static unsigned count;
  
};

unsigned A::next_id;
unsigned A::count;

class B: public A
{
};

LeakCheck::~LeakCheck()
{
  test_out("leakcount: " << A::count);
  for (auto a: as) {
    test_out("leaked: " << a->id);
  }
  assert_(A::count == 0, "leakcount=" << A::count);
}

int main()
{
  std::cout << "Hello\n";
  A a1;
  A a2;
  A a3;
  test_out("foo" << a1.id << a2.id << a3.id);
  assert(a1.get_ref_count() == 1);
  assert(a2.get_ref_count() == 1);
  assert(a3.get_ref_count() == 1);

  A::ptr p1 = &a1;
  assert(a1.get_ref_count() == 2);
  
  auto p2 = A::ptr::create();
  assert(p2.get_ref_count() == 1);
  p2 = p2;
  assert(p2.get_ref_count() == 1);
  A *pp = p2;
  assert(p2.get_ref_count() == 1);
  test_out("id=" << pp->id);

  ptr<B> b1 = ptr<B>::create();
  assert(b1.get_ref_count() == 1);
  
  ptr<A> a4 = b1;
  assert(b1.get_ref_count() == 2);
  ptr<B> b2 = a4.static_cast_to<B>();
  assert(b1.get_ref_count() == 3);

  test_out("bar");
  {
    base::ptr<A> a = base::ptr<B>::create();
    test_out("created a=" << a->id << " with base pointer");
    assert(a.get_ref_count() == 1);
    assert(b1.get_ref_count() == 3);
    a = b1;
    test_out("overwrote with b1=" << b1->id);
    assert_(a.get_ref_count() == 4, a.get_ref_count());
  }
  
  std::cout << "Bye\n";
  return 0;
}

#endif
