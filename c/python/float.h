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
#ifndef dt_PYTHON_FLOAT_h
#define dt_PYTHON_FLOAT_h
#include <Python.h>
#include "python/obj.h"

namespace py {


/**
 * Python float object.
 */
class ofloat : public oobj {
  public:
    ofloat() = default;
    ofloat(const ofloat&) = default;
    ofloat(ofloat&&) = default;
    ofloat& operator=(const ofloat&) = default;
    ofloat& operator=(ofloat&&) = default;

    ofloat(double x);
    ofloat(float x);

    template <typename T> T value() const;

  private:
    // Private constructors, used from `_obj`. If you need to construct
    // `ofloat` from `oobj`, use `oobj.to_pyfloat()` instead.
    ofloat(const robj&);
    ofloat(const oobj&);
    friend class _obj;
};



// Explicit instantiation
template<> float  ofloat::value() const;
template<> double ofloat::value() const;


}  // namespace py

#endif
