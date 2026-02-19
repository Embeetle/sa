

template <typename T> void foo(T t, bool f)
{
  void __failed_assertion();
  if (!f) __failed_assertion();
  (void)t;
}

void bar()
{
  foo<int>(3,true);
}



