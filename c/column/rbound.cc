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
#include "column/rbound.h"
namespace dt {


//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------

static size_t compute_nrows(const colvec& columns) {
  size_t sum_nrows = 0;
  for (const auto& col : columns) {
    sum_nrows += col.nrows();
  }
  return sum_nrows;
}

// TODO: need better mechanism for upcasting
static SType compute_stype(const colvec& columns) {
  SType common_stype = SType::VOID;
  for (const auto& col : columns) {
    common_stype = std::max(common_stype, col.stype());
  }
  return common_stype;
}


Rbound_ColumnImpl::Rbound_ColumnImpl(const colvec& columns)
  : Virtual_ColumnImpl(compute_nrows(columns), compute_stype(columns)),
    columns_(columns)
{
  for (auto& col : columns_) {
    if (col.stype() != stype()) {
      col.cast_inplace(stype());
    }
  }
}


ColumnImpl* Rbound_ColumnImpl::clone() const {
  auto res = new Rbound_ColumnImpl(columns_);
  res->nrows_ = nrows_;
  return res;
}



//------------------------------------------------------------------------------
// Data access
//------------------------------------------------------------------------------

template <typename T>
static inline bool _get(const colvec& columns, size_t i, T* out) {
  for (const auto& col : columns) {
    if (i < col.nrows()) {
      return col.get_element(i, out);
    }
    i -= col.nrows();
  }
  throw ValueError() << "Index " << i << " is out of range";
}

bool Rbound_ColumnImpl::get_element(size_t i, int8_t* out)   const { return _get(columns_, i, out); }
bool Rbound_ColumnImpl::get_element(size_t i, int16_t* out)  const { return _get(columns_, i, out); }
bool Rbound_ColumnImpl::get_element(size_t i, int32_t* out)  const { return _get(columns_, i, out); }
bool Rbound_ColumnImpl::get_element(size_t i, int64_t* out)  const { return _get(columns_, i, out); }
bool Rbound_ColumnImpl::get_element(size_t i, float* out)    const { return _get(columns_, i, out); }
bool Rbound_ColumnImpl::get_element(size_t i, double* out)   const { return _get(columns_, i, out); }
bool Rbound_ColumnImpl::get_element(size_t i, CString* out)  const { return _get(columns_, i, out); }
bool Rbound_ColumnImpl::get_element(size_t i, py::robj* out) const { return _get(columns_, i, out); }




//------------------------------------------------------------------------------
// Materialization
//------------------------------------------------------------------------------

// ColumnImpl* Rbound_ColumnImpl::materialize() {

// }


}  // namespace dt
