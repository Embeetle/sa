#include "hello.h"

int do_something_and_return_an_int();
int only_declared();

typedef unsigned uint32_t;
typedef unsigned long uint64_t;
inline uint64_t __SMLALD (uint32_t op1, uint32_t op2, uint64_t acc)
{
  union llreg_u{
    uint32_t w32[2];
    uint64_t w64;
  } llr;
  llr.w64 = acc;

  asm volatile ("smlald %0, %1, %2, %3" : "=r" (llr.w32[0]), "=r" (llr.w32[1]): "r" (op1), "r" (op2) , "0" (llr.w32[0]), "1" (llr.w32[1]) );

  return(llr.w64);
}

#define DEF 3

struct Coor {
  int x;
  int y;
};

void bar(struct Coor c)
{
  c.y = c.x;
}

struct Coor foo(int argc, char *argv[])
{
  struct Coor r = { argc ? argv[0][0] : 0, 3 };
  return r;
}

static void nxp();

static void nxp();

static void nxp()
{
}

static void nxp();

int main(int argc, char *argv[])
{
  nxp();
  return foo(argc,argv).y + DEF;
}
