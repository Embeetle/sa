size_t x;

/* Any one of these symbols __need_* means that GNU libc
   wants us just to define one data type.  So don't define
   the symbols that indicate this file's entire job has been done.  */
#if (!defined(__need_wchar_t) && !defined(__need_size_t)	\
     && !defined(__need_ptrdiff_t) && !defined(__need_NULL)	\
     && !defined(__need_wint_t)) 
#define _STDDEF_H
#define _STDDEF_H_
/* snaroff@next.com says the NeXT needs this.  */
#define _ANSI_STDDEF_H
#endif

#ifndef __sys_stdtypes_h
/* This avoids lossage on SunOS but only if stdtypes.h comes first.
   There's no way to win with the other order!  Sun lossage.  */

/* On 4.3bsd-net2, make sure ansi.h is included, so we have
   one less case to deal with in the following.  */
#if defined (__BSD_NET2__) || defined (____386BSD____) || (defined (__FreeBSD__) && (__FreeBSD__ < 5)) || defined(__NetBSD__)
#include <machine/ansi.h>
#endif
/* On FreeBSD 5, machine/ansi.h does not exist anymore... */
#if defined (__FreeBSD__) && (__FreeBSD__ >= 5)
#include <sys/_types.h>
#endif

/* In 4.3bsd-net2, machine/ansi.h defines these symbols, which are
   defined if the corresponding type is *not* defined.
   FreeBSD-2.1 defines _MACHINE_ANSI_H_ instead of _ANSI_H_.
   NetBSD defines _I386_ANSI_H_ and _X86_64_ANSI_H_ instead of _ANSI_H_ */
#if defined(_ANSI_H_) || defined(_MACHINE_ANSI_H_) || defined(_X86_64_ANSI_H_)  || defined(_I386_ANSI_H_)
#if !defined(_SIZE_T_) && !defined(_BSD_SIZE_T_)
#define _SIZE_T
#endif
#if !defined(_PTRDIFF_T_) && !defined(_BSD_PTRDIFF_T_)
#define _PTRDIFF_T
#endif
/* On BSD/386 1.1, at least, machine/ansi.h defines _BSD_WCHAR_T_
   instead of _WCHAR_T_. */
#if !defined(_WCHAR_T_) && !defined(_BSD_WCHAR_T_)
#ifndef _BSD_WCHAR_T_
#define _WCHAR_T
#endif
#endif
/* Undef _FOO_T_ if we are supposed to define foo_t.  */
#if defined (__need_ptrdiff_t) || defined (_STDDEF_H_)
#undef _PTRDIFF_T_
#undef _BSD_PTRDIFF_T_
#endif
#if defined (__need_size_t) || defined (_STDDEF_H_)
#undef _SIZE_T_
#undef _BSD_SIZE_T_
#endif
#if defined (__need_wchar_t) || defined (_STDDEF_H_)
#undef _WCHAR_T_
#undef _BSD_WCHAR_T_
#endif
#endif /* defined(_ANSI_H_) || defined(_MACHINE_ANSI_H_) || defined(_X86_64_ANSI_H_) || defined(_I386_ANSI_H_) */

/* Sequent's header files use _PTRDIFF_T_ in some conflicting way.
   Just ignore it.  */
#if defined (__sequent__) && defined (_PTRDIFF_T_)
#undef _PTRDIFF_T_
#endif

/* On VxWorks, <type/vxTypesBase.h> may have defined macros like
   _TYPE_size_t which will typedef size_t.  fixincludes patched the
   vxTypesBase.h so that this macro is only defined if _GCC_SIZE_T is
   not defined, and so that defining this macro defines _GCC_SIZE_T.
   If we find that the macros are still defined at this point, we must
   invoke them so that the type is defined as expected.  */
#if defined (_TYPE_ptrdiff_t) && (defined (__need_ptrdiff_t) || defined (_STDDEF_H_))
_TYPE_ptrdiff_t;
#undef _TYPE_ptrdiff_t
#endif
#if defined (_TYPE_size_t) && (defined (__need_size_t) || defined (_STDDEF_H_))
_TYPE_size_t;
#undef _TYPE_size_t
#endif
#if defined (_TYPE_wchar_t) && (defined (__need_wchar_t) || defined (_STDDEF_H_))
_TYPE_wchar_t;
#undef _TYPE_wchar_t
#endif

/* In case nobody has defined these types, but we aren't running under
   GCC 2.00, make sure that __PTRDIFF_TYPE__, __SIZE_TYPE__, and
   __WCHAR_TYPE__ have reasonable values.  This can happen if the
   parts of GCC is compiled by an older compiler, that actually
   include gstddef.h, such as collect2.  */

/* Unsigned type of `sizeof' something.  */

/* Define this type if we are doing the whole job,
   or if we want this type in particular.  */
#if defined (_STDDEF_H) || defined (__need_size_t)
#ifndef __size_t__	/* BeOS */
#ifndef __SIZE_T__	/* Cray Unicos/Mk */
#ifndef _SIZE_T	/* in case <sys/types.h> has defined it. */
#ifndef _SYS_SIZE_T_H
#ifndef _T_SIZE_
#ifndef _T_SIZE
#ifndef __SIZE_T
#ifndef _SIZE_T_
#ifndef _BSD_SIZE_T_
#ifndef _SIZE_T_DEFINED_
#ifndef _SIZE_T_DEFINED
#ifndef _BSD_SIZE_T_DEFINED_	/* Darwin */
#ifndef _SIZE_T_DECLARED	/* FreeBSD 5 */
#ifndef ___int_size_t_h
#ifndef _GCC_SIZE_T
#ifndef _SIZET_
#ifndef __size_t
typedef long unsigned int size_t; /* line 185 original file */
#endif /* __size_t */
#endif /* _SIZET_ */
#endif /* _GCC_SIZE_T */
#endif /* ___int_size_t_h */
#endif /* _SIZE_T_DECLARED */
#endif /* _BSD_SIZE_T_DEFINED_ */
#endif /* _SIZE_T_DEFINED */
#endif /* _SIZE_T_DEFINED_ */
#endif /* _BSD_SIZE_T_ */
#endif /* _SIZE_T_ */
#endif /* __SIZE_T */
#endif /* _T_SIZE */
#endif /* _T_SIZE_ */
#endif /* _SYS_SIZE_T_H */
#endif /* _SIZE_T */
#endif /* __SIZE_T__ */
#endif /* __size_t__ */
#undef	__need_size_t
#endif /* _STDDEF_H or __need_size_t.  */

#endif /* __sys_stdtypes_h */


