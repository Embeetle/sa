#include "simple.h"

static void foo()
{
}

void bar()
{
}

static void baz();

int main()
{
  int nnn = 42;
  foo();
  bar();
  baz();
  struct Foo {
    int x;
    int y;
    enum Bar { p, q } bar;
  };
  return SIMPLE() + nnn;
}

static void baz()
{
}

void (*getflop())()
{
  return flop;
}
