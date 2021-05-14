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
#ifndef dt_READ_PARSERS_PT_h
#define dt_READ_PARSERS_PT_h
#include <cstdint>  // uint8_t
namespace dt {
namespace read {


/**
  * Parse Type -- each identifier corresponds to one of the
  * parser functions defined in this directory.
  */
enum PT : uint8_t {
  Void,
  Bool01,
  BoolU,
  BoolT,
  BoolL,
  Int32,
  Int32Sep,
  Int64,
  Int64Sep,
  Float32Hex,
  Float64Plain,
  Float64Ext,
  Float64Hex,
  Date32ISO,
  Time64ISO,
  Str32,

  // PT::COUNT is the total number of parser types
  COUNT
};



}}  // namespace dt::read::
#endif
