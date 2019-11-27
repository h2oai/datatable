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
#include "column/const.h"
#include "column/sentinel_fw.h"
#include "column/sentinel_str.h"
#include "parallel/api.h"
#include "python/_all.h"
namespace dt {



ConstNa_ColumnImpl::ConstNa_ColumnImpl(size_t nrows, SType stype)
  : Const_ColumnImpl(nrows, stype) {}



bool ConstNa_ColumnImpl::get_element(size_t, int8_t*)   const { return false; }
bool ConstNa_ColumnImpl::get_element(size_t, int16_t*)  const { return false; }
bool ConstNa_ColumnImpl::get_element(size_t, int32_t*)  const { return false; }
bool ConstNa_ColumnImpl::get_element(size_t, int64_t*)  const { return false; }
bool ConstNa_ColumnImpl::get_element(size_t, float*)    const { return false; }
bool ConstNa_ColumnImpl::get_element(size_t, double*)   const { return false; }
bool ConstNa_ColumnImpl::get_element(size_t, CString*)  const { return false; }
bool ConstNa_ColumnImpl::get_element(size_t, py::robj*) const { return false; }


ColumnImpl* ConstNa_ColumnImpl::clone() const {
  return new ConstNa_ColumnImpl(nrows_, stype_);
}


void ConstNa_ColumnImpl::na_pad(size_t nrows, Column&) {
  xassert(nrows >= nrows_);
  nrows_ = nrows;
}


//------------------------------------------------------------------------------
// Materializing
//------------------------------------------------------------------------------

template <typename T, typename ColClass>
static Column _fw_col(size_t nrows) {
  Buffer buf = Buffer::mem(nrows * sizeof(T));
  T* data = static_cast<T*>(buf.xptr());

  parallel_for_static(nrows,
    [=](size_t i) {
      data[i] = GETNA<T>();
    });

  if (std::is_same<T, PyObject*>::value) {
    Py_None->ob_refcnt += nrows;
    buf.set_pyobjects(/* clear_data= */ false);
  }
  return Column(new ColClass(nrows, std::move(buf)));
}

template <typename T>
static Column _str_col(size_t nrows) {
  Buffer offsets = Buffer::mem((nrows + 1) * sizeof(T));
  T* offsets_data = static_cast<T*>(offsets.xptr());
  *offsets_data++ = 0;

  parallel_for_static(nrows,
    [=](size_t i) {
      offsets_data[i] = GETNA<T>();
    });
  return Column(new SentinelStr_ColumnImpl<T>(nrows, std::move(offsets), Buffer()));
}


void ConstNa_ColumnImpl::materialize(Column& out, bool) {
  switch (stype_) {
    case SType::VOID:
    case SType::BOOL:    out = _fw_col<int8_t,  SentinelBool_ColumnImpl>(nrows_); break;
    case SType::INT8:    out = _fw_col<int8_t,  SentinelFw_ColumnImpl<int8_t>>(nrows_); break;
    case SType::INT16:   out = _fw_col<int16_t, SentinelFw_ColumnImpl<int16_t>>(nrows_); break;
    case SType::INT32:   out = _fw_col<int32_t, SentinelFw_ColumnImpl<int32_t>>(nrows_); break;
    case SType::INT64:   out = _fw_col<int64_t, SentinelFw_ColumnImpl<int64_t>>(nrows_); break;
    case SType::FLOAT32: out = _fw_col<float,   SentinelFw_ColumnImpl<float>>(nrows_); break;
    case SType::FLOAT64: out = _fw_col<double,  SentinelFw_ColumnImpl<double>>(nrows_); break;
    case SType::OBJ:     out = _fw_col<PyObject*, SentinelObj_ColumnImpl>(nrows_); break;
    case SType::STR32:   out = _str_col<uint32_t>(nrows_); break;
    case SType::STR64:   out = _str_col<uint64_t>(nrows_); break;
    default:
      throw NotImplError() << "Cannot materialize NaColumn of type " << stype_;
  }
}




}  // namespace dt
