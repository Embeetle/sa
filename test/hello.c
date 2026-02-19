#ifndef _include_hello_c
#define _include_hello_c

static int foo(int i, int j);

#include "hello.c"

static int foo(int i, int j)
{
}

#define FOO foo

void bar()
{
}

int main()
{
  FOO(7,6);
  barr();
  int ali = 5;
  ++ali;
  return ali-6;
}

#endif
