#include "A.h"

int main()
{
  B bob;
  bob.blob();
  A &aha = bob;
  aha.blob();
  return 0;
}
