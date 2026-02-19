#include "simple.h"
#include "simple2.h"

typedef int an_integer;

typedef struct
{
  int __count;
  union
  {
    long __wch;
    unsigned char __wchb[4];
  } __value;		/* Value so far.  */
} _mbstate_t;


int simple()
{
  return simple2;
}
