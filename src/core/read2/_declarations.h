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
//
// The *read process begins with the user calling function `py_fread()` from
// python (see "py_fread.cc"). The function accepts many different parameters,
// so the first thing we do is parse/validate those parameters. Also, the
// parameters fall into 2 categories: those describing _what_ to read, and
// those that tell _how_ to read.
//
// The "what" parameters are then converted into a `SourceIterable` object,
// while "how" parameters are collected into `ParseOptions`. Both of these
// objects are combined to create a `ReadDirector`, which then assumes the
// central role in the process.
//
// The entry point for ReadDirector class are `readSingle()` / `readNext()`
// methods. The first one is used by `fread()`, the second by `iread()`. Both
// methods are very similar, with the only difference that `readSingle()` emits
// a warning/error if there is more than one input source.
//
// When `readNext()` is called, the first thing it does is determines the
// relevant `Source*`. This could be either the Source left over from the
// previous call to `readNext()`, or a new source retrieved from the
// `SourceIterator`.
//
//
//
//------------------------------------------------------------------------------
#ifndef dt_READ2_DECLARATIONS_h
#define dt_READ2_DECLARATIONS_h
#include "_dt.h"
namespace dt {
namespace read2 {


class BufferedStream;
class ReadDirector;
class ReadOptions;
class Source;
class SourceIterator;
class Stream;


enum class SeparatorKind : int8_t {
  AUTO,       // auto-detect, this is the default
  NONE,       // read input in single-column mode
  CHAR,       // single-character separator
  STRING,     // multi-character separator
  WHITESPACE, // separator is the regex /\s+/
  // in the future, arbitrary regex separators may also be separated
};




}}  // namespace dt::read2
#endif
