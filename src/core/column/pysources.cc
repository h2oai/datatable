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
#include "column/pysources.h"
#include "python/dict.h"
#include "python/tuple.h"
#include "utils/assert.h"
namespace dt {



//------------------------------------------------------------------------------
// PyList_ColumnImpl
//------------------------------------------------------------------------------

PyList_ColumnImpl::PyList_ColumnImpl(const py::olist& list)
  : Virtual_ColumnImpl(list.size(), SType::OBJ),
    list_(list) {}


ColumnImpl* PyList_ColumnImpl::clone() const {
  return new PyList_ColumnImpl(list_);
}


bool PyList_ColumnImpl::get_element(size_t i, py::robj* out) const {
  xassert(i < nrows_);
  *out = list_[i];
  return true;
}




//------------------------------------------------------------------------------
// PyTupleList_ColumnImpl
//------------------------------------------------------------------------------

PyTupleList_ColumnImpl::PyTupleList_ColumnImpl(const py::olist& list, size_t j)
  : Virtual_ColumnImpl(list.size(), SType::OBJ),
    tuple_list_(list),
    index_(j) {}


ColumnImpl* PyTupleList_ColumnImpl::clone() const {
  return new PyTupleList_ColumnImpl(tuple_list_, index_);
}


bool PyTupleList_ColumnImpl::get_element(size_t i, py::robj* out) const {
  xassert(i < nrows_);
  *out = py::rtuple::unchecked(tuple_list_[i])[index_];
  return true;
}




//------------------------------------------------------------------------------
// PyDictList_ColumnImpl
//------------------------------------------------------------------------------

PyDictList_ColumnImpl::PyDictList_ColumnImpl(const py::olist& list, py::oobj k)
  : Virtual_ColumnImpl(list.size(), SType::OBJ),
    dict_list_(list),
    key_(k) {}


ColumnImpl* PyDictList_ColumnImpl::clone() const {
  return new PyDictList_ColumnImpl(dict_list_, key_);
}


bool PyDictList_ColumnImpl::get_element(size_t i, py::robj* out) const {
  xassert(i < nrows_);
  *out = py::rdict::unchecked(dict_list_[i]).get_or_none(key_);
  return true;
}





}  // namespace dt
