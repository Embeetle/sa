#ifndef __A_h
#define __A_h

#define FOO 1
#undef FOO
#define FOO 2

class B {
  void foo() { bar(); }
  virtual int bar() { return 42; }
};
class C {
public:
  virtual void blob() = 0;
};

class A: public C, protected virtual B {
public:
  A();
  void blob();

#if 0
  // blib() is never defined
  void blib();

  // blub calls blib, but is inline and is never called -> should be fine.  No
  // code is generated for blub, so there is no undefined reference to blib.
  // BTW: the same is true for a non-method inline function.
  void blub() { blib(); }
#endif
};

#endif

