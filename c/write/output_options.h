//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#ifndef dt_WRITE_OUTPUT_OPTIONS_h
#define dt_WRITE_OUTPUT_OPTIONS_h
#include <cstdint>   // int8_t
namespace dt {
namespace write {

// These constants coincide with those defined in the python `csv` module.
enum class Quoting : int8_t {
  MINIMAL = 0,
  ALL = 1,
  NONNUMERIC = 2,
  NONE = 3
};


struct output_options {
  // Not all of these are currently effective
  bool compress_zlib;
  bool floats_as_hex;
  bool integers_as_hex;
  bool booleans_as_words;
  bool strings_never_quote;
  bool strings_always_quote;
  bool strings_escape_quotes;
  Quoting quoting_mode;

  output_options()
    : compress_zlib(false),
      floats_as_hex(false),
      integers_as_hex(false),
      booleans_as_words(false),
      strings_never_quote(false),
      strings_always_quote(false),
      strings_escape_quotes(false),
      quoting_mode(Quoting::MINIMAL) {}
};


static_assert(sizeof(output_options) == 8,
              "Unexpected size of output_options");


}}  // namespace dt::write
#endif
