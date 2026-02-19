#include <stdint.h>

int main()
{
  uint32_t fpscr = 0;
  __asm volatile ("VMSR fpscr, %0" : : "r" (fpscr) : "vfpcc");
  return 0;
}
