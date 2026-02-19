#include <stdint.h>

extern uint32_t __StackTop;
extern uint32_t __StackLimit;

void SystemInit(void)
{
}

int main()
{
  return __StackTop - __StackLimit;
}
