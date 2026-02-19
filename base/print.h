// Copyright 2018-2023 Johan Cockx
#ifndef __base_print_h
#define __base_print_h

#include <sstream>

#include <iostream>

namespace base {

  
  //--------------------------------------------------------------------------
  // Convert any printable value to a string.
  template <typename Printable> inline std::string print( const Printable &i )
  {
    std::ostringstream temp;
    temp << i;
    return temp.str();
  }
}

//------------------------------------------------------------------------------
// Specialization for floating point types: make the string as short as possible
// while still reproducing the original binary floating point value on input.
//------------------------------------------------------------------------------

#include "float_manip.h"
#include "debug.h"
#include <iomanip>
#include <cstdlib>

namespace base {

  //--------------------------------------------------------------------------
  template <> inline std::string print( const float &i )
  {
    // Assuming float is IEEE single precision or similar, such that each
    // distinct value can be uniquely represented with 9 significant digits.
    // The assert below is only an approximation of this assumption.
    assert(sizeof(float)==4);
    std::ostringstream temp;
    temp << std::setprecision(9) << i;
    std::string longval = temp.str();
    std::string shortval = round_last_significant_digit(longval);
    // Comparison below is a floating point comparison, not a bit pattern
    // comparison as it should be.  How to implement a bit pattern comparison
    // here?  Anyway, the result is the same, even for NANs, although possibly
    // slightly less efficient.
    return (float)(atof(shortval.data())) == i ? shortval : longval;
  }

  //--------------------------------------------------------------------------
  template <> inline std::string print( const double &i )
  {
    // Assuming double is IEEE doubleprecision or similar, such that each
    // distinct value can be uniquely represented with 17 significant digits.
    // The assert below is only an approximation of this assumption.
    assert(sizeof(double)==8);
    std::ostringstream temp;
    temp << std::setprecision(17) << i;
    std::string longval = temp.str();
    std::string shortval = round_last_significant_digit(longval);
    // Comparison below is a floating point comparison, not a bit pattern
    // comparison as it should be.  How to implement a bit pattern comparison
    // here?  Anyway, the result is the same, even for NANs, although possibly
    // slightly less efficient.
    return atof(shortval.data()) == i ? shortval : longval;
  }

  //--------------------------------------------------------------------------
  template <> inline std::string print( const long double &i )
  {
    // Assuming double is either a 128 bit or similar, such that each distinct
    // value can be uniquely represented with 31 significant digits, or just a
    // synonym for double.  The assert below is only an approximation of this
    // assumption.  Note that some compilers use 80 bits IEEE double extended
    // precision to implement long double, which requires fewer digits; in that
    // case, the code below will not always result in the shortest
    // representation, although it will be correct.
    assert(sizeof(long double)<=16);
    std::ostringstream temp;
    temp << std::setprecision(sizeof(long double) <= 8 ? 17 : 31) << i;
    std::string longval = temp.str();
    std::string shortval = round_last_significant_digit(longval);
    // Comparison below is a floating point comparison, not a bit pattern
    // comparison as it should be.  How to implement a bit pattern comparison
    // here?  Anyway, the result is the same, even for NANs, although possibly
    // slightly less efficient.
    std::istringstream temp2(shortval.data());
    long double s;
    temp2 >> s;
    return s == i ? shortval : longval;
  }

  //--------------------------------------------------------------------------
  // Long double is not supported, as its precision and corresponding required
  // number of digits vary too much between platforms, and there is no simple
  // way to automatically derive the required number of digits.  On purpose, no
  // implementation is provided here; using this template results in a link
  // error, which seems preferable to a run time error such as an assert.  A
  // compile time error would be even better.
  template <> std::string print( const long double &i );
}

#endif

