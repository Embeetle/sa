
void foo();

void foo() __attribute__((weak));

void foo();

int main()
{
  if (foo) foo();
  return 0;
}
