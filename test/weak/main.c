
void foo() __attribute__((weak));

int main()
{
  if (foo) foo();
  return 0;
}
