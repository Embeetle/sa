#if defined (__clang__)
  #pragma clang system_header   /* treat file as system include file */
#endif

__attribute__((always_inline)) static inline int fun()
{
  return 42;
}

// A static inline function (in C99) should generate a declaration, not a
// definition.  The body is only used for inlining. A definition requires
// "extern inline".  If the function is not called, access to an undefined
// external symbol (such as bar_data below) should *not* result in a linker
// error.
extern const int bar_data;
/*__attribute__((always_inline)) */ static inline int bar()
{
  return bar_data;
}

// This function is called,  so a definition for tip_data is required.
extern const int tip_data;
/*__attribute__((always_inline)) */ static inline int tip()
{
  return tip_data;
}

