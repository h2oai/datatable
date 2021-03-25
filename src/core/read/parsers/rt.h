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
#ifndef dt_READ_PARSERS_RT_h
#define dt_READ_PARSERS_RT_h
#include <cstdint>   // uint8_t
namespace dt {
namespace read {


/**
  * Requested Type -- column type as requested by the user; each may correspond
  * to one or more parse types.
  *
  * Do not use "enum class" here: we want this enum to be implicitly
  * convertible into integers, so that we can use it as an array index.
  */
enum RT : uint8_t {
  RDrop    = 0,
  RAuto    = 1,
  RBool    = 2,
  RInt     = 3,
  RInt32   = 4,
  RInt64   = 5,
  RFloat   = 6,
  RFloat32 = 7,
  RFloat64 = 8,
  RStr     = 9,
  RStr32   = 10,
  RStr64   = 11,
};




}}  // namespace dt::read::
#endif
