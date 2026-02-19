template<class T, class L> 
auto min(const T& a, const L& b) -> decltype((b < a) ? b : a)
{
  return (b < a) ? b : a;
}

int main()
{
  unsigned char p = 'x';
  long q = 33;
  return min(p,q);
}
