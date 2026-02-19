typedef unsigned size_t;

class __make_unsigned_selector_base {
protected:
  template<typename...> struct _List { };

  template<typename _Tp, typename... _Up>
  struct _List<_Tp, _Up...> : _List<_Up...> {
    static constexpr size_t __size = sizeof(_Tp);
  };
};
