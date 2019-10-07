//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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
#include <memory>
#include "column/range.h"
#include "utils/exceptions.h"
namespace dt {



//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

static size_t compute_nrows(int64_t start, int64_t stop, int64_t step) {
  xassert(step != 0);
  int64_t length = (step > 0) ? (stop - start + step - 1) / step
                              : (start - stop - step - 1) / (-step);
  return (length < 0) ? 0 : static_cast<size_t>(length);
}

static SType compute_stype(int64_t start, int64_t stop, SType stype) {
  if (stype == SType::VOID) {
    bool start_is_int32 = (start == static_cast<int32_t>(start));
    bool stop_is_int32  = (stop == static_cast<int32_t>(stop));
    return (start_is_int32 && stop_is_int32) ? SType::INT32 : SType::INT64;
  }
  auto ltype = info(stype).ltype();
  if (ltype == LType::INT || ltype == LType::REAL) {
    return stype;
  }
  throw ValueError() << "Invalid stype " << stype << " for a range column";
}



Range_ColumnImpl::Range_ColumnImpl(int64_t start, int64_t stop, int64_t step,
                                   SType stype)
  : Virtual_ColumnImpl(compute_nrows(start, stop, step),
                       compute_stype(start, stop, stype)),
    start_(start),
    step_(step) {}


// private constructor (used for cloning)
Range_ColumnImpl::Range_ColumnImpl(size_t nrows, SType stype, int64_t start,
                                   int64_t step)
  : Virtual_ColumnImpl(nrows, stype),
    start_(start),
    step_(step) {}



ColumnImpl* Range_ColumnImpl::clone() const {
  return new Range_ColumnImpl(nrows_, stype_, start_, step_);
}




//------------------------------------------------------------------------------
// Element access
//------------------------------------------------------------------------------

template <typename T>
inline bool Range_ColumnImpl::_get(size_t i, T* out) const {
  *out = static_cast<T>(start_ + static_cast<int64_t>(i) * step_);
  return true;
}

bool Range_ColumnImpl::get_element(size_t i, int8_t* out)  const { return _get(i, out); }
bool Range_ColumnImpl::get_element(size_t i, int16_t* out) const { return _get(i, out); }
bool Range_ColumnImpl::get_element(size_t i, int32_t* out) const { return _get(i, out); }
bool Range_ColumnImpl::get_element(size_t i, int64_t* out) const { return _get(i, out); }
bool Range_ColumnImpl::get_element(size_t i, float* out)   const { return _get(i, out); }
bool Range_ColumnImpl::get_element(size_t i, double* out)  const { return _get(i, out); }




//------------------------------------------------------------------------------
// Materialization
//------------------------------------------------------------------------------

// void Range_ColumnImpl::materialize(Column& out) {

// }



}  // namespace dt
