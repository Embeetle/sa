// Copyright 2018-2023 Johan Cockx
#include "float_manip.h"
#include <cctype>

//------------------------------------------------------------------------------
std::string base::round_last_significant_digit(const std::string &value)
{
  const char *base = value.data();
  const char *begin = base;
  if (*begin == '-') begin++;
  //
  // Find last significant digit after decimal point.  If there is no decimal
  // point, just return the original string.
  const char *tail = begin;
  if (*tail == '-') tail++;
  if (!isdigit(*tail)) return value;
  do { tail++; } while (isdigit(*tail));
  if (*tail != '.') return value;
  do { tail++; } while (isdigit(*tail));
  const char *last = tail-1;
  //
  // Skip any trailing zero's.
  while (*last == '0') last--;
  //
  // Round down last digit <= 4, remove trailing decimal point.
  if (*last == '.') {
    return std::string(base,last-base) + std::string(tail);
  } else if (*last == '1' || *last == '2' || *last == '3' || *last == '4') {
    do { last--; } while (*last == '0');
    if (*last != '.') last++;
    return std::string(base,last-base) + std::string(tail);
  }
  //
  // Round up last digit >= 5 and propagate.
  do { last--; } while (*last == '9');
  if (*last != '.') {
    return std::string(base,last-base) + std::string(1,*last+1)
      + std::string(tail);
  }
  const char *dot = last;
  last--;
  while (*last == '9') {
    if (last == begin) {
      // Propagated over first significant digit: requires special treatment.
      std::string sign(base,begin-base);
      if (*tail == 'e' || *tail == 'E') {
        // Try to remove one zero before the decimal point by changing the
        // exponent.
        const char *begin_exp = tail+1;
        if (*begin_exp == '-') {
          begin_exp++;
          if (isdigit(*begin_exp) && *begin_exp != '0') {
            //
            // Increment negative exponent -> decrement absolute value.
            const char *tail_exp = begin_exp;
            do { tail_exp++; } while (isdigit(*tail_exp));
            const char *digit_exp = tail_exp-1;
            while (*digit_exp == '0') digit_exp--;
            if (digit_exp == begin_exp && *digit_exp == '1') {
              // Exponent becomes zero,  so remove it completely
              return sign + "1" + std::string(dot-begin-1,'0')
                + std::string(tail_exp);
            }
            return sign + "1" + std::string(dot-begin-1,'0')
              + std::string(tail,digit_exp-tail)
              + std::string(1,*digit_exp-1)
              + std::string(tail_exp-digit_exp-1,'9')
              + std::string(tail_exp);
          }
        } else {
          if (isdigit(*begin_exp) && *begin_exp != '0') {
            //
            // Increment positive exponent
            const char *tail_exp = begin_exp;
            do { tail_exp++; } while (isdigit(*tail_exp));
            const char *digit_exp = tail_exp-1;
            while (*digit_exp == '9') {
              if (digit_exp == begin_exp) {
                //
                // Need extra digit in exponent
                return sign + "1" + std::string(dot-begin-1,'0')
                  + std::string(tail,digit_exp-tail)
                  + "1"
                  + std::string(tail_exp-digit_exp,'0')
                  + std::string(tail_exp);
              }
              digit_exp--;
            }
            return sign + "1" + std::string(dot-begin-1,'0')
              + std::string(tail,digit_exp-tail)
              + std::string(1,*digit_exp+1)
              + std::string(tail_exp-digit_exp-1,'0')
              + std::string(tail_exp);
          }
        }
      }
      return sign + "1" + std::string(dot-begin,'0') + std::string(tail);
    }
    last--;
  }
  return std::string(base,last-base) + std::string(1,*last+1)
    + std::string(dot-last-1,'0') + std::string(tail);
}

#ifdef SELFTEST

#include <sstream>

//--------------------------------------------------------------------------
template <typename Printable> inline std::string print( const Printable &i )
{
  std::ostringstream temp;
  temp << i;
  return temp.str();
}

#include <cfloat>
#include <cmath>
#include <climits>
#include <iostream>

//------------------------------------------------------------------------------
int main()
{
  std::string v[] = {
    "1",
    print(FLT_EPSILON),
    print(1+FLT_EPSILON),
    "-1",
    "-1.0001",
    "-0.9999",
    print(-1-FLT_EPSILON),
    "2",
    print(2+FLT_EPSILON),
    "0.29999999",
    "0.3",
    "3.14",
    "1e32",
    "1e-32",
    "1e-40",
    print(INFINITY),
    print(-INFINITY),
    print(NAN),
    "99.5",
    "99.95",
    "99.85",
    "9.8",
    "-99.5",
    "-99.95",
    "-99.85",
    "-9.8",
    "-9.8e13",
    "-9.8e9",
    "-9.8e39",
    "-9.8e99",
    "-9.8e-33",
    "-9.8e-320",
    "-9.8e-30",
    "-9.8e-2",
    "-9.8e-1",
    "-9.8e0",
  };
  for (int i = 0; i < sizeof(v)/sizeof(v[0]); ++i) {
    std::cout << v[i] << " ---> " << shorten(v[i]) << "\n";
  }
}

#endif
