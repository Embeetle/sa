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
