//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#include <cmath>
#include "python/float.h"
#include "utils/exceptions.h"

namespace py {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

ofloat::ofloat(double src) {
  v = std::isnan(src)? py::None().release()
                     : PyFloat_FromDouble(src);  // new ref
}

ofloat::ofloat(float src) : ofloat(static_cast<double>(src)) {}


// private constructor
ofloat::ofloat(const robj& src) : oobj(src) {}



//------------------------------------------------------------------------------
// Value conversions
//------------------------------------------------------------------------------

template<>
float ofloat::value() const noexcept {
  if (!v) return GETNA<float>();
  return static_cast<float>(PyFloat_AS_DOUBLE(v));
}

template<>
double ofloat::value() const noexcept {
  if (!v) return GETNA<double>();
  return PyFloat_AS_DOUBLE(v);
}



}  // namespace py
