// Copyright 2018-2023 Johan Cockx
#include "Chain.h"
#include <iostream>

class Foo: public base::Chain<Foo> {
public:
  int n;
  Foo(int n): n(n) {}
  Foo(): base::Chain<Foo>(this), n(0) {}
};

std::ostream &operator<<(std::ostream &out, const Foo &foo)
{
  return out << "Foo(" << foo.n << ")";
}

static void print_chain(base::Chain<Foo> &chain)
{
  for (Foo *p = chain.next; p != &chain; p = p->next) {
    std::cout << " " << *p;
  }
  std::cout << "\n";
}

int main()
{
  std::cout << "Hello\n";
  Foo chain;
  
  Foo x(5);
  Foo y(6);
  std::cout << x << " " << y << "\n";

  x.insert_after(&chain);
  y.insert_after(&chain);
  print_chain(chain);
  x.remove();
  print_chain(chain);
  x.insert_after(&y);
  print_chain(chain);
  y.remove();
  print_chain(chain);
  y.insert_after(&x);
  print_chain(chain);
  return 0;
}
