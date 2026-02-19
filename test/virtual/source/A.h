#ifndef __A_h
#define __A_h

class A {
public:
  A();
  virtual void blob() = 0;
};

class B: public A {
public:
  void blob();
};

class C: public B {
public:
  void blob();
};

#endif
