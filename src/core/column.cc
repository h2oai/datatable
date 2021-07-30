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
#include <cstdlib>     // atoll
#include "column/const.h"
#include "column/sentinel.h"
#include "column/virtual.h"
#include "python/_all.h"
#include "python/string.h"
#include "utils/assert.h"
#include "utils/file.h"
#include "utils/misc.h"
#include "column.h"
#include "datatablemodule.h"
#include "rowindex.h"
#include "sort.h"



Column Column::new_data_column(size_t nrows, dt::SType stype) {
  return dt::Sentinel_ColumnImpl::make_column(nrows, stype);
}


Column Column::new_na_column(size_t nrows, dt::SType stype) {
  return Column(new dt::ConstNa_ColumnImpl(nrows, stype));
}


Column Column::new_mbuf_column(size_t nrows, dt::SType stype, Buffer&& mbuf) {
  return dt::Sentinel_ColumnImpl::make_fw_column(nrows, stype, std::move(mbuf));
}


Column Column::new_string_column(
    size_t nrows, Buffer&& data, Buffer&& str)
{
  return dt::Sentinel_ColumnImpl::make_str_column(
                      nrows, std::move(data), std::move(str));
}



//------------------------------------------------------------------------------
// Column constructors
//------------------------------------------------------------------------------

void swap(Column& lhs, Column& rhs) {
  std::swap(lhs.impl_, rhs.impl_);
}

Column::Column() noexcept
  : impl_(nullptr) {}

Column::Column(dt::ColumnImpl*&& col) noexcept
  : impl_(col) {}

Column::Column(const Column& other) noexcept {
  _acquire_impl(other.impl_);
}

Column::Column(Column&& other) noexcept : Column() {
  std::swap(impl_, other.impl_);
}

Column& Column::operator=(const Column& other) {
  auto old = impl_;
  _acquire_impl(other.impl_);
  _release_impl(old);
  return *this;
}

Column& Column::operator=(Column&& other) noexcept {
  auto old = impl_;
  impl_ = other.impl_;
  other.impl_ = nullptr;
  _release_impl(old);
  return *this;
}

Column::~Column() {
  _release_impl(impl_);
}


dt::ColumnImpl* Column::release() && {
  xassert(impl_->refcount_ == 1);
  auto tmp = const_cast<dt::ColumnImpl*>(impl_);
  impl_ = nullptr;
  return tmp;
}

void Column::_acquire_impl(const dt::ColumnImpl* impl) {
  impl_ = impl;
  impl_->refcount_ += 1;
}

void Column::_release_impl(const dt::ColumnImpl* impl) {
  if (!impl) return;
  impl->refcount_ -= 1;
  if (impl->refcount_ == 0) {
    delete impl;
  }
}

dt::ColumnImpl* Column::_get_mutable_impl(bool keep_stats) {
  xassert(impl_);
  if (impl_->refcount_ > 1) {
    auto newimpl = impl_->clone();
    if (newimpl->stats_) {
      if (!keep_stats) newimpl->stats_ = nullptr;
    }
    else if (keep_stats && impl_->stats_) {
      newimpl->stats_ = impl_->stats_->clone();
      newimpl->stats_->column = newimpl;
    }
    impl_->refcount_ -= 1;
    impl_ = newimpl;
  } else {
    if (!keep_stats) reset_stats();
  }
  xassert(impl_->refcount_ == 1);
  return const_cast<dt::ColumnImpl*>(impl_);
}

bool Column::operator==(const Column& other) const noexcept {
  return impl_ == other.impl_;
}



//------------------------------------------------------------------------------
// Properties
//------------------------------------------------------------------------------

size_t Column::nrows() const noexcept {
  return impl_->nrows();
}

size_t Column::na_count() const {
  return stats()->nacount();
}

const dt::Type& Column::type() const noexcept {
  return impl_->type_;
}

dt::SType Column::stype() const noexcept {
  return impl_->type_.stype();
}

dt::LType Column::ltype() const noexcept {
  return dt::stype_to_ltype(impl_->stype());
}

bool Column::is_fixedwidth() const noexcept {
  return !stype_is_variable_width(impl_->stype());
}

bool Column::is_virtual() const noexcept {
  return impl_->is_virtual();
}

bool Column::allow_parallel_access() const {
  return impl_->allow_parallel_access();
}

bool Column::is_constant() const noexcept {
  return bool(dynamic_cast<const dt::Const_ColumnImpl*>(impl_));
}

size_t Column::elemsize() const noexcept {
  return stype_elemsize(impl_->stype());
}

Column::operator bool() const noexcept {
  return (impl_ != nullptr);
}

size_t Column::memory_footprint() const noexcept {
  return sizeof(Column) + (impl_? impl_->memory_footprint() : 0);
}



//------------------------------------------------------------------------------
// Column : data accessors
//------------------------------------------------------------------------------

bool Column::get_element(size_t i, int8_t* out) const {
  xassert(i < nrows());
  return impl_->get_element(i, out);
}

bool Column::get_element(size_t i, int16_t* out) const {
  xassert(i < nrows());
  return impl_->get_element(i, out);
}

bool Column::get_element(size_t i, int32_t* out) const {
  xassert(i < nrows());
  return impl_->get_element(i, out);
}

bool Column::get_element(size_t i, int64_t* out) const {
  xassert(i < nrows());
  return impl_->get_element(i, out);
}

bool Column::get_element(size_t i, float* out) const {
  xassert(i < nrows());
  return impl_->get_element(i, out);
}

bool Column::get_element(size_t i, double* out) const {
  xassert(i < nrows());
  return impl_->get_element(i, out);
}

bool Column::get_element(size_t i, dt::CString* out) const {
  xassert(i < nrows());
  return impl_->get_element(i, out);
}

bool Column::get_element(size_t i, py::oobj* out) const {
  xassert(i < nrows());
  return impl_->get_element(i, out);
}

bool Column::get_element(size_t i, Column* out) const {
  xassert(i < nrows());
  return impl_->get_element(i, out);
}



template <typename T>
static inline py::oobj getelem(const Column& col, size_t i) {
  T x;
  bool isvalid = col.get_element(i, &x);
  return isvalid? py::oobj::wrap(x) : py::None();
}

py::oobj Column::get_element_as_pyobject(size_t i) const {
  switch (stype()) {
    case dt::SType::VOID:    return py::None();
    case dt::SType::BOOL: {
      int8_t x;
      bool isvalid = get_element(i, &x);
      return isvalid? py::obool(x) : py::None();
    }
    case dt::SType::INT8:    return getelem<int8_t>(*this, i);
    case dt::SType::INT16:   return getelem<int16_t>(*this, i);
    case dt::SType::INT32:   return getelem<int32_t>(*this, i);
    case dt::SType::INT64:   return getelem<int64_t>(*this, i);
    case dt::SType::FLOAT32: return getelem<float>(*this, i);
    case dt::SType::FLOAT64: return getelem<double>(*this, i);
    case dt::SType::STR32:
    case dt::SType::STR64:   return getelem<dt::CString>(*this, i);
    case dt::SType::DATE32: {
      int32_t x;
      bool isvalid = get_element(i, &x);
      return isvalid? py::odate(x) : py::None();
    }
    case dt::SType::TIME64: {
      int64_t x;
      bool isvalid = get_element(i, &x);
      return isvalid? py::odatetime(x) : py::None();
    }
    case dt::SType::OBJ: {
      py::oobj x;
      bool isvalid = get_element(i, &x);
      return isvalid? x : py::None();
    }
    case dt::SType::ARR32:
    case dt::SType::ARR64: {
      Column elem;
      bool isvalid = get_element(i, &elem);
      if (isvalid) {
        auto n = elem.nrows();
        auto res = py::olist(n);
        for (size_t j = 0; j < n; j++) {
          res.set(j, elem.get_element_as_pyobject(j));
        }
        return std::move(res);
      } else {
        return py::None();
      }
    }
    default:
      throw NotImplError() << "Unable to convert elements of type `"
        << type() << "` into python objects";
  }
}

bool Column::get_element_isvalid(size_t i) const {
  switch (stype()) {
    case dt::SType::VOID: return false;
    case dt::SType::BOOL:
    case dt::SType::INT8: {
      int8_t x;
      return get_element(i, &x);
    }
    case dt::SType::INT16: {
      int16_t x;
      return get_element(i, &x);
    }
    case dt::SType::DATE32:
    case dt::SType::INT32: {
      int32_t x;
      return get_element(i, &x);
    }
    case dt::SType::TIME64:
    case dt::SType::INT64: {
      int64_t x;
      return get_element(i, &x);
    }
    case dt::SType::FLOAT32: {
      float x;
      return get_element(i, &x);
    }
    case dt::SType::FLOAT64: {
      double x;
      return get_element(i, &x);
    }
    case dt::SType::STR32:
    case dt::SType::STR64: {
      dt::CString x;
      return get_element(i, &x);
    }
    default:
      throw NotImplError() << "Unable to check validity of the element "
        << "for stype: `" << stype() << "`";
  }
}



//------------------------------------------------------------------------------
// Column : data buffers
//------------------------------------------------------------------------------

NaStorage Column::get_na_storage_method() const noexcept {
  return impl_->get_na_storage_method();
}


size_t Column::get_num_data_buffers() const noexcept {
  return impl_->get_num_data_buffers();
}


bool Column::is_data_editable(size_t k) const {
  return impl_->is_data_editable(k);
}


size_t Column::get_data_size(size_t k) const {
  return impl_->get_data_size(k);
}


const void* Column::get_data_readonly(size_t k) const {
  return impl_->get_data_readonly(k);
}


void* Column::get_data_editable(size_t k) {
  auto pcol = _get_mutable_impl();
  return pcol->get_data_editable(k);
}


Buffer Column::get_data_buffer(size_t k) const {
  return impl_->get_data_buffer(k);
}




//------------------------------------------------------------------------------
// Column : manipulation
//------------------------------------------------------------------------------

void Column::materialize(bool to_memory) {
  if (impl_->is_virtual()) {
    auto pcol = _get_mutable_impl(/* keep_stats= */ true);
    pcol->materialize(*this, to_memory);
  }
  else if (to_memory) {
    // A non-virtual column doesn't need to be materialized, unless
    // it's to bring the memory-mapped data into RAM. However, in
    // that case we're not really changing the column, so no need to
    // acquire the "mutable impl", which may incur unnecessary data
    // copy.
    const_cast<dt::ColumnImpl*>(impl_)->materialize(*this, to_memory);
  }
}

void Column::replace_values(const RowIndex& replace_at,
                            const Column& replace_with)
{
  materialize();
  auto pcol = _get_mutable_impl();
  pcol->replace_values(replace_at, replace_with, *this);
}


void Column::repeat(size_t ntimes) {
  auto pcol = _get_mutable_impl();
  pcol->repeat(ntimes, *this);
}

void Column::apply_rowindex(const RowIndex& ri) {
  if (!ri) return;
  auto pcol = _get_mutable_impl();
  pcol->apply_rowindex(ri, *this);  // modifies in-place
}

void Column::resize(size_t new_nrows) {
  size_t curr_nrows = nrows();
  if (new_nrows == curr_nrows) return;
  auto pcol = _get_mutable_impl();
  if (new_nrows > curr_nrows) pcol->na_pad(new_nrows, *this);
  if (new_nrows < curr_nrows) pcol->truncate(new_nrows, *this);
}


void Column::sort_grouped(const Groupby& grps) {
  auto pcol = _get_mutable_impl();
  pcol->sort_grouped(grps, *this);
}


void Column::verify_integrity() const {
  impl_->verify_integrity();
}


void Column::fill_npmask(bool* out_mask, size_t row0, size_t row1) const {
  impl_->fill_npmask(out_mask, row0, row1);
}



void Column::cast_inplace(dt::Type new_type) {
  if (!new_type || new_type == type()) return;
  impl_->cast_replace(new_type, *this);
}


Column Column::cast(dt::Type new_type) const {
  Column newcol(*this);
  newcol.cast_inplace(new_type);
  return newcol;
}

void Column::replace_type_unsafe(dt::Type new_type) {
  _get_mutable_impl()->type_ = new_type;
}


// old API
void Column::cast_inplace(dt::SType new_stype) {
  cast_inplace(dt::Type::from_stype(new_stype));
}
Column Column::cast(dt::SType new_stype) const {
  return cast(dt::Type::from_stype(new_stype));
}
