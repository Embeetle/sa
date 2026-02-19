
// Stdint and  type_traits contains a lot of templates that used to cause issues.
#include <stdint.h>
#include <type_traits>

static_assert(sizeof(uint32_t) == 4, "wrong uint32_t size");
static_assert(sizeof(long unsigned int) == 4, "wrong long unsigned int size");

#include "A.h"

// _GNU_SOURCE is not defined in g++ by default;  it is in Clang
#ifdef _GNU_SOURCE
static_assert(0, "_GNU_SOURCE defined");
#endif

inline int foo()
{
  return 0;
}

static int bar()
{
  return 0;
}

int baz()
{
  return 0;
}

// Check that instantiating a class generates a call to the constructor
// (either a static instance or an automatic one).
struct A aha;

// Use an external symbol define in another file, that also includes
// type_traits. Type_traits especially causes problems when included more than
// once in the program, because it has static template member variables that
// look like global variable definitions.
extern int num;

int (*foo2)() = foo;

//typedef int (*foo_like)();
//foo_like foo3()
int (*(foo3()))()
{
  return foo2;
}

class TwoWire {
public:
  static const uint32_t TWI_CLOCK = 100000;
};

int main(int argc, char **argv)
{
  union { uint16_t val; struct { uint8_t lsb; uint8_t msb; }; } in, out;
   
  // This call should not be necessary to include A.cpp
  //aha.blob();
  foo2();
  foo3()();
  C *ccc = &aha;
  ccc->blob();
  return foo() + bar() + baz() + num + TwoWire::TWI_CLOCK;
}

