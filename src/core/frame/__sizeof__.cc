//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include "column/sentinel_str.h"
#include "documentation.h"
#include "frame/py_frame.h"
#include "python/_all.h"
namespace py {



//------------------------------------------------------------------------------
// Frame::__sizeof__
//------------------------------------------------------------------------------

static PKArgs args__sizeof__(
  0, 0, 0, false, false, {}, "__sizeof__", dt::doc_Frame___sizeof__);


oobj Frame::m__sizeof__(const PKArgs&) {
  size_t sz = dt->memory_footprint();
  sz += sizeof(*this);
  return oint(sz);
}


void Frame::_init_sizeof(XTypeMaker& xt) {
  xt.add(METHOD(&Frame::m__sizeof__, args__sizeof__));
}



}  // namespace py

//------------------------------------------------------------------------------
// DataTable methods
//------------------------------------------------------------------------------

size_t DataTable::memory_footprint() const noexcept {
  size_t sz = 0;
  sz += sizeof(*this);
  sz += sizeof(Column) * columns_.capacity();
  sz += sizeof(std::string) * names_.capacity();
  for (size_t i = 0; i < ncols_; ++i) {
    sz += columns_[i].memory_footprint();
    sz += names_[i].size();
  }
  if (py_names_) {
    sz += py_names_.get_sizeof();
    sz += py_inames_.get_sizeof();
    for (size_t i = 0; i < ncols_; ++i) {
      sz += py_names_[i].get_sizeof();
    }
  }
  return sz;
}


template <typename T>
size_t dt::SentinelStr_ColumnImpl<T>::memory_footprint() const noexcept {
  return sizeof(*this)
         + offbuf_.memory_footprint()
         + strbuf_.memory_footprint()
         + (stats_? stats_->memory_footprint() : 0);
}


template class dt::SentinelStr_ColumnImpl<uint32_t>;
template class dt::SentinelStr_ColumnImpl<uint64_t>;
