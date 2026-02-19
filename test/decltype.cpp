namespace arduino {
  class String {
    String(double value, unsigned char decimalPlaces);
  };

  template<class T, class L> 
  auto min(const T& a, const L& b) -> decltype((b < a) ? b : a)
  {
    return (b < a) ? b : a;
  }

  template<class T, class L> 
  auto max(const T& a, const L& b) -> decltype((b < a) ? b : a)
  {
    return (a < b) ? b : a;
  }
  
  void foo(unsigned char decimalPlaces)
  {
	decimalPlaces = min(decimalPlaces, 3);
  }

  String::String(double value, unsigned char decimalPlaces)
  {
	decimalPlaces = min(decimalPlaces, 3);
  }
}
