template<class T> class A {
public:
  void foo() {
    void bar();  // (1)
    bar();
  }
};

void bar() {} // (2)
// could also be 'void bar();' if bar is defined elsewhere

template<class T> class B {
public:
  void foo() {
    void bar(); // (3)
    bar();
  }
};

int main()
{
  A<int> a; a.foo();
  B<int> b; b.foo();
  return 0;
}
