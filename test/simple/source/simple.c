#include "simple.h"
#include <simple2.h>

int simple()
{
  return simple2;
}

// Declaration after inline definition emits a strong definition of flop.
void flop();
