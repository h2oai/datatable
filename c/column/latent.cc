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
#include "column/latent.h"
#include "column.h"
#include "column_impl.h"
namespace dt {

#define CHECK_SIZE(cls) \
  static_assert(sizeof(Latent_ColumnImpl) >= sizeof(cls), \
                "Size of Latent_ColumnImpl is smaller than size of " #cls)
CHECK_SIZE(VoidColumn);
CHECK_SIZE(BoolColumn);
CHECK_SIZE(IntColumn<int8_t>);
CHECK_SIZE(IntColumn<int16_t>);
CHECK_SIZE(IntColumn<int32_t>);
CHECK_SIZE(IntColumn<int64_t>);
CHECK_SIZE(FwColumn<float>);
CHECK_SIZE(FwColumn<double>);
CHECK_SIZE(StringColumn<uint32_t>);
CHECK_SIZE(StringColumn<uint64_t>);
CHECK_SIZE(PyObjectColumn);



Latent_ColumnImpl::Latent_ColumnImpl(ColumnImpl* pcol)
  : ColumnImpl(pcol->nrows(), pcol->stype()),
    column(pcol) {}


bool Latent_ColumnImpl::is_virtual() const noexcept {
  return true;
}


ColumnImpl* Latent_ColumnImpl::shallowcopy() const {
  return new Latent_ColumnImpl(column->shallowcopy());
}

ColumnImpl* Latent_ColumnImpl::materialize() {
  return vivify();
}


bool Latent_ColumnImpl::get_element(size_t i, int8_t* out)   const { return vivify()->get_element(i, out); }
bool Latent_ColumnImpl::get_element(size_t i, int16_t* out)  const { return vivify()->get_element(i, out); }
bool Latent_ColumnImpl::get_element(size_t i, int32_t* out)  const { return vivify()->get_element(i, out); }
bool Latent_ColumnImpl::get_element(size_t i, int64_t* out)  const { return vivify()->get_element(i, out); }
bool Latent_ColumnImpl::get_element(size_t i, float* out)    const { return vivify()->get_element(i, out); }
bool Latent_ColumnImpl::get_element(size_t i, double* out)   const { return vivify()->get_element(i, out); }
bool Latent_ColumnImpl::get_element(size_t i, CString* out)  const { return vivify()->get_element(i, out); }
bool Latent_ColumnImpl::get_element(size_t i, py::robj* out) const { return vivify()->get_element(i, out); }


ColumnImpl* Latent_ColumnImpl::vivify() const {
  // Note: we're using placement-materialize to overwrite `this` with
  // the materialized `column` object. Effectively, the current object
  // will be replaced with a new one. The destructor of current object
  // is never called though, so we need to delete the original column
  // instance manually.
  // This will work, provided that `sizeof(*this)` is >= to the size
  // of the class of the materialized column.
  ColumnImpl* ptr = const_cast<ColumnImpl*>(
                        static_cast<const ColumnImpl*>(this));
  std::unique_ptr<ColumnImpl> old = std::move(column);
  old->materialize_at(ptr);
  xassert(!is_virtual());
  return ptr;
}



}  // namespace dt
