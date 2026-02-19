class Bam {
public:
  Bam() {}
};

Bam operator+(const Bam &b1, const Bam &b2) 
{
  return Bam();
}

void add()
{
  Bam b1;
  Bam b2;
  Bam b3 = b1 + b2;
}

template<class T1, class T2, int I>
class AAA {};           // primary template
 
template<class T, int I>
class AAA<T, T*, I> {}; // #1: partial specialization where T2 is a pointer to T1
 
template<class T, class T2, int I>
class AAA<T*, T2, I> {};// #2: partial specialization where T1 is a pointer
 
template<class T>
class AAA<int, T*, 5> {}; // #3: partial specialization where
                        //     T1 is int, I is 5, and T2 is a pointer
 
template<class X, class T, int I>
class AAA<X, T*, I> {};   // #4: partial specialization where T2 is a pointer

template<> class AAA<double, float*, 7> {};

// Explicit instantiation
template class AAA<int, int, 8>;


AAA<float, double, 3> fd3;
AAA<unsigned, unsigned*, 4> uup4;
AAA<char*, int, 5> cpi5;
AAA<double, float*, 6> dfp6;

template <int _TYPE>
class Foo {
public:
  Foo() {}

  ~Foo() {}

  void kill(int n = 1) {}

  static int xxx;

  void plain();

  void foo() {
    void plain();
    plain();
  }
};

// For an explanation of how plain() is treated and linked to the declarations
// in Foo::foo() and Foo1::foo(), read Clang.cpp comments in 'EntityKind
// get_entity_kind(clang::FunctionDecl *decl)'
void plain() {}

template <int _TYPE>
class Foo2 {
public:
  void foo() {
    void plain();
    plain();
  }
};

template <int _TYPE> int Foo<_TYPE>::xxx = _TYPE;

Foo<2> oled;
Foo<2> oled2;
Foo<3> oled3;

class Bar {
public:
  virtual void foo(int n = 1) {}
};

template<class T> const void *ptr(const T &t) { return &t; }
template<class B> const void *ptr(B *p ) { return p; }

template<class T>
constexpr T pi = T(3.1415926535897932385L);

template <typename Type>
static int printFieldT(Bar* file, char sign, Type value, char term)
{
  return term;
}

namespace Colour {
  typedef char Code;
}

namespace Catch {
  namespace {

        struct IColourImpl {
            virtual ~IColourImpl() = default;
            virtual void use( Colour::Code _colourCode ) = 0;
        };

        struct NoColourImpl : IColourImpl {
            void use( Colour::Code ) override {}

            static IColourImpl* instance() {
                static NoColourImpl s_instance;
                return &s_instance;
            }
        };

  } // anon namespace
} // namespace Catch

int main(int argc, char *argv[])
{
  oled2.kill();
  Bar bar;
  Bar bar2 = Bar();
  Bar bar3 = Bar();
  if (pi<float> > 3) {
    bar.foo();
  }
  if (ptr(bar)) { return 0; }
  if (ptr(&bar)) { return 0; }

  printFieldT(&bar, 0, argc, 'z');

  Foo<3> foo;
  foo.foo();
  
  return argc + Foo<2>::xxx + Foo<3>::xxx;
}
