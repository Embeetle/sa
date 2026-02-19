//extern "C" {

  template<typename T> void foo(T t)
  {
    void __failed_assertion();
    __failed_assertion();
  }

void __failed_assertion();

  template<typename T> void bar(T t)
  {
    void __failed_assertion();
    __failed_assertion();
  }

//}

int top()
{
  //foo<int>(4);
  //bar<int>(4);
  return 0;
}
