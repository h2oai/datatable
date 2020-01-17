//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
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



Column Column::new_data_column(size_t nrows, SType stype) {
  return dt::Sentinel_ColumnImpl::make_column(nrows, stype);
}


Column Column::new_na_column(size_t nrows, SType stype) {
  return Column(new dt::ConstNa_ColumnImpl(nrows, stype));
}


Column Column::new_mbuf_column(size_t nrows, SType stype, Buffer&& mbuf) {
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
    xassert(!newimpl->stats_);
    if (keep_stats && impl_->stats_) {
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



//------------------------------------------------------------------------------
// Properties
//------------------------------------------------------------------------------

size_t Column::nrows() const noexcept {
  return impl_->nrows_;
}

size_t Column::na_count() const {
  return stats()->nacount();
}

SType Column::stype() const noexcept {
  return impl_->stype_;
}

LType Column::ltype() const noexcept {
  return info(impl_->stype_).ltype();
}

bool Column::is_fixedwidth() const noexcept {
  return !info(impl_->stype_).is_varwidth();
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
  return info(impl_->stype_).elemsize();
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

bool Column::get_element(size_t i, CString* out) const {
  xassert(i < nrows());
  return impl_->get_element(i, out);
}

bool Column::get_element(size_t i, py::robj* out) const {
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
    case SType::BOOL: {
      int8_t x;
      bool isvalid = get_element(i, &x);
      return isvalid? py::obool(x) : py::None();
    }
    case SType::INT8:    return getelem<int8_t>(*this, i);
    case SType::INT16:   return getelem<int16_t>(*this, i);
    case SType::INT32:   return getelem<int32_t>(*this, i);
    case SType::INT64:   return getelem<int64_t>(*this, i);
    case SType::FLOAT32: return getelem<float>(*this, i);
    case SType::FLOAT64: return getelem<double>(*this, i);
    case SType::STR32:
    case SType::STR64:   return getelem<CString>(*this, i);
    case SType::OBJ:     return getelem<py::robj>(*this, i);
    case SType::VOID:    return py::None();
    default:
      throw NotImplError() << "Unable to convert elements of stype "
          << stype() << " into python objects";
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
  auto pcol = _get_mutable_impl(/* keep_stats= */ true);
  pcol->materialize(*this, to_memory);
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
