#include "A.h"

// This file - in automatic mode - should not be included, although it does
// contain a weak definition of B::foo() and B::bar().  B::foo() and B::bar()
// are defined inline in A.h, and such an inline definition constitutes a weak
// definition according to C++ semantics. B::bar() is used in foo.cpp (which
// also includes A.h), when it instantiates 'struct A aha'. Struct A derives
// from class B, and the address of 'aha' is taken, so 'aha.bar()' could be
// called.

void should_not_be_included()
{
}
