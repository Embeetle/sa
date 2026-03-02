// Copyright © 2018-2026 Johan Cockx
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// SPDX-License-Identifier: GPL-3.0-or-later

/*
  Functions to manipulate floating point numbers in text form.
*/

#ifndef __base_float_manip_h
#define __base_float_manip_h

#include <string>

namespace base {

  // Shorten a floating point number by rounding its last significant digit and
  // removing any resulting trailing zeros.
  //
  // This is intended to be used when converting floating point numbers to a
  // string representation.  Many implementations of floating point to string
  // conversion, including the standard I/O and LLVM implementations, generate a
  // representation that is correct but longer than necessary to be able to
  // regenerate the original floating point number during reverse conversion.
  // For example, 0.3 is generated as 0.299999999 or 0.30000001, which is
  // correct in the sense that the binary representation is the same as that of
  // 0.3, but harder to read and unnecessarily long.
  //
  // The idea is to round the last significant digit of a floating point number
  // after conversion to string, and than convert back to the binary
  // representation.  If this yields exactly the original number, the rounded
  // representation is preferred over the original one.
  //
  // The complete algorithm cannot be implemented here because it depends on the
  // binary representation used.  The most complex part, however, is the
  // (textual) rounding, and this is provided here.
  //
  // There exist algorithms to directly generate the shortest string
  // representation from the binary representation; see for example SW03
  // (overview), G90 (technical report), and BD96 (improvement on G90).  One can
  // even freely download an implementation of G90: see for example
  // http://www.loni.ucla.edu/twiki/bin/view/CCB/ShapeToolAsciiStorage.
  // However, the described algorithms are surprisingly complex and hard to
  // implement, and the downloadable implementation is not easy to understand.
  // It is also not clear how general the implementation is in terms of
  // different floating point representations.  In comparison, the text based
  // implementation provided here is relatively simple and completely
  // independent of the binary floating point format used.
  //
  // Very useful further background on floating point operations, including
  // binary to decimal conversion, can be found in
  // http://docs.oracle.com/cd/E19957-01/806-3568/ncg_goldberg.html.
  //
  std::string round_last_significant_digit(const std::string &value);
  
}

#endif

