struct Foo {
  static int foo;
};

typedef __SIZE_TYPE__ 	size_t;
typedef __PTRDIFF_TYPE__	ptrdiff_t;
template<typename _Tp, _Tp __v>
struct integral_constant
{
  static constexpr _Tp                  value = __v;
  typedef _Tp                           value_type;
  typedef integral_constant<_Tp, __v>   type;
  constexpr operator value_type() const noexcept { return value; }
  constexpr value_type operator()() const noexcept { return value; }
};

template<typename _Tp, _Tp __v>
constexpr _Tp integral_constant<_Tp, __v>::value;

/// The type used as a compile-time boolean with true value.
typedef integral_constant<bool, true>     true_type;

/// The type used as a compile-time boolean with false value.
typedef integral_constant<bool, false>    false_type;

template<typename>
struct remove_cv;

template<typename>
struct __is_integral_helper
  : public false_type { };

template<>
struct __is_integral_helper<bool>
  : public true_type { };

template<>
struct __is_integral_helper<char>
  : public true_type { };

template<>
struct __is_integral_helper<signed char>
  : public true_type { };

template<>
struct __is_integral_helper<unsigned char>
  : public true_type { };

#ifdef _GLIBCXX_USE_WCHAR_T
template<>
struct __is_integral_helper<wchar_t>
  : public true_type { };
#endif

#ifdef _GLIBCXX_USE_CHAR8_T
template<>
struct __is_integral_helper<char8_t>
  : public true_type { };
#endif

template<>
struct __is_integral_helper<char16_t>
  : public true_type { };

template<>
struct __is_integral_helper<char32_t>
  : public true_type { };

template<>
struct __is_integral_helper<short>
  : public true_type { };

template<>
struct __is_integral_helper<unsigned short>
  : public true_type { };

template<>
struct __is_integral_helper<int>
  : public true_type { };

template<>
struct __is_integral_helper<unsigned int>
  : public true_type { };

template<>
struct __is_integral_helper<long>
  : public true_type { };

template<>
struct __is_integral_helper<unsigned long>
  : public true_type { };

template<>
struct __is_integral_helper<long long>
  : public true_type { };

template<>
struct __is_integral_helper<unsigned long long>
  : public true_type { };

// Conditionalizing on __STRICT_ANSI__ here will break any port that
// uses one of these types for size_t.
#if defined(__GLIBCXX_TYPE_INT_N_0)
template<>
struct __is_integral_helper<__GLIBCXX_TYPE_INT_N_0>
  : public true_type { };

template<>
struct __is_integral_helper<unsigned __GLIBCXX_TYPE_INT_N_0>
  : public true_type { };
#endif
#if defined(__GLIBCXX_TYPE_INT_N_1)
template<>
struct __is_integral_helper<__GLIBCXX_TYPE_INT_N_1>
  : public true_type { };

template<>
struct __is_integral_helper<unsigned __GLIBCXX_TYPE_INT_N_1>
  : public true_type { };
#endif
#if defined(__GLIBCXX_TYPE_INT_N_2)
template<>
struct __is_integral_helper<__GLIBCXX_TYPE_INT_N_2>
  : public true_type { };

template<>
struct __is_integral_helper<unsigned __GLIBCXX_TYPE_INT_N_2>
  : public true_type { };
#endif
#if defined(__GLIBCXX_TYPE_INT_N_3)
template<>
struct __is_integral_helper<__GLIBCXX_TYPE_INT_N_3>
  : public true_type { };

template<>
struct __is_integral_helper<unsigned __GLIBCXX_TYPE_INT_N_3>
  : public true_type { };
#endif

/// is_integral
template<typename _Tp>
struct is_integral
  : public __is_integral_helper<typename remove_cv<_Tp>::type>::type
{ };

/// is_enum
template<typename _Tp>
struct is_enum
  : public integral_constant<bool, __is_enum(_Tp)>
{ };

/// is_const
template<typename>
struct is_const
  : public false_type { };

template<typename _Tp>
struct is_const<_Tp const>
  : public true_type { };

/// is_volatile
template<typename>
struct is_volatile
  : public false_type { };

template<typename _Tp>
struct is_volatile<_Tp volatile>
  : public true_type { };

// Sign modifications.

// Utility for constructing identically cv-qualified types.
template<typename _Unqualified, bool _IsConst, bool _IsVol>
struct __cv_selector;

template<typename _Unqualified>
struct __cv_selector<_Unqualified, false, false>
{ typedef _Unqualified __type; };

template<typename _Unqualified>
struct __cv_selector<_Unqualified, false, true>
{ typedef volatile _Unqualified __type; };

template<typename _Unqualified>
struct __cv_selector<_Unqualified, true, false>
{ typedef const _Unqualified __type; };

template<typename _Unqualified>
struct __cv_selector<_Unqualified, true, true>
{ typedef const volatile _Unqualified __type; };

template<typename _Qualified, typename _Unqualified,
         bool _IsConst = is_const<_Qualified>::value,
         bool _IsVol = is_volatile<_Qualified>::value>
class __match_cv_qualifiers
{
  typedef __cv_selector<_Unqualified, _IsConst, _IsVol> __match;

public:
  typedef typename __match::__type __type;
};

// Utility for finding the unsigned versions of signed integral types.
template<typename _Tp>
struct __make_unsigned
{ typedef _Tp __type; };

template<>
struct __make_unsigned<char>
{ typedef unsigned char __type; };

template<>
struct __make_unsigned<signed char>
{ typedef unsigned char __type; };

template<>
struct __make_unsigned<short>
{ typedef unsigned short __type; };

template<>
struct __make_unsigned<int>
{ typedef unsigned int __type; };

template<>
struct __make_unsigned<long>
{ typedef unsigned long __type; };

template<>
struct __make_unsigned<long long>
{ typedef unsigned long long __type; };

#if defined(__GLIBCXX_TYPE_INT_N_0)
template<>
struct __make_unsigned<__GLIBCXX_TYPE_INT_N_0>
{ typedef unsigned __GLIBCXX_TYPE_INT_N_0 __type; };
#endif
#if defined(__GLIBCXX_TYPE_INT_N_1)
template<>
struct __make_unsigned<__GLIBCXX_TYPE_INT_N_1>
{ typedef unsigned __GLIBCXX_TYPE_INT_N_1 __type; };
#endif
#if defined(__GLIBCXX_TYPE_INT_N_2)
template<>
struct __make_unsigned<__GLIBCXX_TYPE_INT_N_2>
{ typedef unsigned __GLIBCXX_TYPE_INT_N_2 __type; };
#endif
#if defined(__GLIBCXX_TYPE_INT_N_3)
template<>
struct __make_unsigned<__GLIBCXX_TYPE_INT_N_3>
{ typedef unsigned __GLIBCXX_TYPE_INT_N_3 __type; };
#endif

// Select between integral and enum: not possible to be both.
template<typename _Tp,
         bool _IsInt = is_integral<_Tp>::value,
         bool _IsEnum = is_enum<_Tp>::value>
class __make_unsigned_selector;

template<typename _Tp>
class __make_unsigned_selector<_Tp, true, false>
{
  using __unsigned_type
  = typename __make_unsigned<typename remove_cv<_Tp>::type>::__type;

public:
  using __type
  = typename __match_cv_qualifiers<_Tp, __unsigned_type>::__type;
};

class __make_unsigned_selector_base
{
protected:
  template<typename...> struct _List { };

  template<typename _Tp, typename... _Up>
  struct _List<_Tp, _Up...> : _List<_Up...>
  { static constexpr size_t __size = sizeof(_Tp); };

  template<size_t _Sz, typename _Tp, bool = (_Sz <= _Tp::__size)>
  struct __select;

  template<size_t _Sz, typename _Uint, typename... _UInts>
  struct __select<_Sz, _List<_Uint, _UInts...>, true>
  { using __type = _Uint; };

  template<size_t _Sz, typename _Uint, typename... _UInts>
  struct __select<_Sz, _List<_Uint, _UInts...>, false>
    : __select<_Sz, _List<_UInts...>>
  { };
};

// Choose unsigned integer type with the smallest rank and same size as _Tp
template<typename _Tp>
class __make_unsigned_selector<_Tp, false, true>
  : __make_unsigned_selector_base
{
  // With -fshort-enums, an enum may be as small as a char.
  using _UInts = _List<unsigned char, unsigned short, unsigned int,
                       unsigned long, unsigned long long>;

  using __unsigned_type = typename __select<sizeof(_Tp), _UInts>::__type;

public:
  using __type
  = typename __match_cv_qualifiers<_Tp, __unsigned_type>::__type;
};

template<>
struct __make_unsigned<wchar_t>
{
  using __type
  = typename __make_unsigned_selector<wchar_t, false, true>::__type;
};

inline void flop() {}
