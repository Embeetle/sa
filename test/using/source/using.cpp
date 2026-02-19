
class A {
public:
  void write() {}
  void write(int) {};
  void write(float) {};
};

class B: public A {
public:
  using A::write;
  void write(int) {};
};

int main()
{
  B b;
  b.write();
}
