// In C99, "inline" yields an inline-only definition and "extern inline" yields
// a global definition that may also be expanded inline.

inline int tip() { return 1; }
extern inline int top() { return 2; }
extern int tap();
inline int tup() { return 1; }
static inline __attribute__((always_inline)) int tep() { return 6; }


int main()
{
  return tip() + tap() + top() + (int)&tep;
}

//int tip();
