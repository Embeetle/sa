extern int a;
int b = 2;
int c;
static int d = 4;
static int e;
static int e = 7;

extern void ff(int);
int main(int argc, char *argv[])
{
  (void)argv;
  int f = argc - 1;
  static int g = 7;
  extern int h;
  (void)h;
  register int i = 9;
  ff(i);
  return a + b + c + d + e + f + g + h + i + __ARM_EABI__;
}

static void f();
static void f() {}

typedef int T;
T z;
