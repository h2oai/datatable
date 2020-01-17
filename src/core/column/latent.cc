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
#include "column/sentinel_fw.h"
#include "column/sentinel_str.h"
#include "parallel/api.h"
#include "column.h"
namespace dt {

#define CHECK_SIZE(cls) \
  static_assert(sizeof(Latent_ColumnImpl) >= sizeof(cls), \
                "Size of Latent_ColumnImpl is smaller than size of " #cls)
CHECK_SIZE(SentinelBool_ColumnImpl);
CHECK_SIZE(SentinelFw_ColumnImpl<int8_t>);
CHECK_SIZE(SentinelFw_ColumnImpl<int16_t>);
CHECK_SIZE(SentinelFw_ColumnImpl<int32_t>);
CHECK_SIZE(SentinelFw_ColumnImpl<int64_t>);
CHECK_SIZE(SentinelFw_ColumnImpl<float>);
CHECK_SIZE(SentinelFw_ColumnImpl<double>);
CHECK_SIZE(SentinelStr_ColumnImpl<uint32_t>);
CHECK_SIZE(SentinelStr_ColumnImpl<uint64_t>);
CHECK_SIZE(SentinelObj_ColumnImpl);



Latent_ColumnImpl::Latent_ColumnImpl(ColumnImpl* pcol)
  : Virtual_ColumnImpl(pcol->nrows(), pcol->stype()),
    column_(std::move(pcol)) {}


Latent_ColumnImpl::Latent_ColumnImpl(const Column& col)
  : Virtual_ColumnImpl(col.nrows(), col.stype()),
    column_(col) {}



ColumnImpl* Latent_ColumnImpl::clone() const {
  return new Latent_ColumnImpl(column_);
}

void Latent_ColumnImpl::materialize(Column&, bool to_memory) {
  vivify(to_memory);
}


bool Latent_ColumnImpl::allow_parallel_access() const {
  return vivify()->allow_parallel_access();
}


bool Latent_ColumnImpl::get_element(size_t i, int8_t* out)   const { return vivify()->get_element(i, out); }
bool Latent_ColumnImpl::get_element(size_t i, int16_t* out)  const { return vivify()->get_element(i, out); }
bool Latent_ColumnImpl::get_element(size_t i, int32_t* out)  const { return vivify()->get_element(i, out); }
bool Latent_ColumnImpl::get_element(size_t i, int64_t* out)  const { return vivify()->get_element(i, out); }
bool Latent_ColumnImpl::get_element(size_t i, float* out)    const { return vivify()->get_element(i, out); }
bool Latent_ColumnImpl::get_element(size_t i, double* out)   const { return vivify()->get_element(i, out); }
bool Latent_ColumnImpl::get_element(size_t i, CString* out)  const { return vivify()->get_element(i, out); }
bool Latent_ColumnImpl::get_element(size_t i, py::robj* out) const { return vivify()->get_element(i, out); }


// This method is not thread-safe!
// In particular, if multiple threads are trying to retrieve
// different elements at the same time, then each thread will call
// the `vivify()` function simultaneously, resulting in UB.
// It might be possible to fix this by adding a mutex; however it
// will still be problematic:
//   - if only 1 thread is working on materialization, while others
//     are waiting, it will be inefficient;
//   - if the materializing function tries to use parallelism, then
//     either an exception will be thrown, or a deadlock will occur.
//
// Thus, a better approach is needed: some mechanism to inform the
// caller that this column must be "prepared" first.

ColumnImpl* Latent_ColumnImpl::vivify(bool to_memory) const {
  xassert(num_threads_in_team() == 0);
  // Note: we're using placement-materialize to overwrite `this` with
  // the materialized `column` object. Effectively, the current object
  // will be replaced with a new one.
  // This will work, provided that `sizeof(*this)` is >= the size
  // of the class of the materialized column.
  auto ptr = const_cast<void*>(static_cast<const void*>(this));
  column_.materialize(to_memory);
  ColumnImpl* new_pcol = std::move(column_).release();
  SType stype = new_pcol->stype();

  switch (stype) {
    case SType::BOOL:    new (ptr) SentinelBool_ColumnImpl(std::move(new_pcol)); break;
    case SType::INT8:    new (ptr) SentinelFw_ColumnImpl<int8_t>(std::move(new_pcol)); break;
    case SType::INT16:   new (ptr) SentinelFw_ColumnImpl<int16_t>(std::move(new_pcol)); break;
    case SType::INT32:   new (ptr) SentinelFw_ColumnImpl<int32_t>(std::move(new_pcol)); break;
    case SType::INT64:   new (ptr) SentinelFw_ColumnImpl<int64_t>(std::move(new_pcol)); break;
    case SType::FLOAT32: new (ptr) SentinelFw_ColumnImpl<float>(std::move(new_pcol)); break;
    case SType::FLOAT64: new (ptr) SentinelFw_ColumnImpl<double>(std::move(new_pcol)); break;
    default:
      throw NotImplError() << "Cannot vivify column of type " << stype;
  }
  return static_cast<ColumnImpl*>(ptr);
}



}  // namespace dt
