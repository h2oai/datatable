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
#ifndef dt_READ2_READ_OPTIONS_h
#define dt_READ2_READ_OPTIONS_h
#include "read2/_declarations.h"
#include "utils/logger.h"
namespace dt {
namespace read2 {


enum SeparatorKind : int8_t {
  AUTO,       // auto-detect, this is the default
  NONE,       // read input in single-column mode
  CHAR,       // single-character separator
  STRING,     // multi-character separator
  WHITESPACE, // separator is the regex /\s+/
  // in the future we may also support regex separator
};


class ReadOptions {
  private:
    dt::log::Logger logger_;
    std::string     separator_;
    SeparatorKind   separatorKind_;
    size_t : 56;

  public:
    ReadOptions();

    dt::log::Logger& logger();

    void initLogger(const py::Arg& logger, const py::Arg& verbose);
    void initSeparator(const py::Arg& separator);
};




}}  // namespace dt::read2
#endif
