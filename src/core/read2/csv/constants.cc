//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#include "read2/_declarations.h"
namespace dt {
namespace read2 {



// Prohibited seps:
//   * «\n» (0x0A), «\r» (0x0D)
//   * «"» (0x22), «'» (0x27), «\» (0x5C), «`» (0x60)
//   * «(», «)» (0x28, 0x29), «<», «>» (0x3C, 0x3E), «[», «]» (0x5B, 0x5D),
//     «{», «}» (0x7B, 0x7D)
//   * «+» (0x2B), «-» (0x2D), «.» (0x2E)
//   * «0» .. «9» (0x30 .. 0x39)
//   * «A» .. «Z» (0x41 .. 0x5A), «a» .. «z» (0x61 .. 0x7A)
// Seps with elevated priority:
//   * «,» (0x2C), «\t» (0x09), «;» (0x3B), «|» (0x7C)
// Seps with reduced priority:
//   * «@» (0x40), «^» (0x5E), «_» (0x5F), « » (0x20), «:» (0x3A)
// Seps with lowest priority:
//   * «!» (0x21), «#» (0x23), «$» (0x24), «%» (0x25), «&» (0x26), «*» (0x2A),
//     «=» (0x3D), «?» (0x3F)
//
const int8_t separatorLikelihood[128] = {
  // 1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
  5, 5, 5, 5, 5, 5, 5, 5, 5, 8, 0, 5, 5, 0, 5, 5,  // 00 .. 0F
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,  // 10 .. 1F
  3, 1, 0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 9, 0, 0, 5,  // 20 .. 2F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 7, 0, 1, 0, 1,  // 30 .. 3F
  4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 40 .. 4F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4,  // 50 .. 5F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 60 .. 6F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 5, 5,  // 70 .. 7F
};



}}  // namespace dt::read2
