// Tip and top are inline functions, so they will only be emitted when actually
// called.  Only tip is called, so only tip_data requires a definition. This
// should work, even if the definitions of tip and top appear after the call to
// tip.

// C++ inline function test. Global variable top_data is only used in top()
// which is never called, so no error should result

extern int tip_data;
extern int top_data;

int tip();
int top();

class Foo {
public:
  void init();
  void foo() { bar(); }
  virtual int bar() { return 42; }
};

int main()
{
  Foo foo;
  foo.init();
  foo.foo();
  return tip();
}

inline int tip() { return tip_data; }
inline int top() { return top_data; }
int tip();

inline void Foo::init() {}
