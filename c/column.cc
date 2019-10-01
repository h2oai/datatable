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
#include "column_impl.h"
#include "datatablemodule.h"
#include "rowindex.h"
#include "sort.h"



Column Column::new_data_column(size_t nrows, SType stype) {
  return Column(dt::Sentinel_ColumnImpl::make_column(nrows, stype));
}


Column Column::new_na_column(size_t nrows, SType stype) {
  return Column(new dt::ConstNa_ColumnImpl(nrows, stype));
}


Column Column::new_mbuf_column(size_t nrows, SType stype, Buffer&& mbuf) {
  return Column(dt::Sentinel_ColumnImpl::make_fw_column(
                    nrows, stype, std::move(mbuf)));
}


Column Column::new_string_column(
    size_t nrows, Buffer&& data, Buffer&& str)
{
  return Column(dt::Sentinel_ColumnImpl::make_str_column(
                      nrows, std::move(data), std::move(str)));
}



//------------------------------------------------------------------------------
// Column constructors
//------------------------------------------------------------------------------

void swap(Column& lhs, Column& rhs) {
  std::swap(lhs.pcol, rhs.pcol);
}

Column::Column()
  : pcol(nullptr) {}

Column::Column(ColumnImpl*&& col)
  : pcol(col) {}

Column::Column(const Column& other)
  : pcol(other.pcol->acquire_instance()) {}

Column::Column(Column&& other) noexcept : Column() {
  std::swap(pcol, other.pcol);
}

Column& Column::operator=(const Column& other) {
  auto old = pcol;
  pcol = other.pcol->acquire_instance();
  if (old) old->release_instance();
  return *this;
}

Column& Column::operator=(Column&& other) noexcept {
  auto old = pcol;
  pcol = other.pcol;
  other.pcol = nullptr;
  if (old) old->release_instance();
  return *this;
}

Column::~Column() {
  if (pcol) pcol->release_instance();
}


ColumnImpl* Column::release() noexcept {
  ColumnImpl* tmp = pcol;
  pcol = nullptr;
  return tmp;
}



//---- Properties ----------------------

size_t Column::nrows() const noexcept {
  return pcol->nrows_;
}

size_t Column::na_count() const {
  return stats()->nacount();
}

SType Column::stype() const noexcept {
  return pcol->stype_;
}

LType Column::ltype() const noexcept {
  return info(pcol->stype_).ltype();
}

bool Column::is_fixedwidth() const noexcept {
  return !info(pcol->stype_).is_varwidth();
}

bool Column::is_virtual() const noexcept {
  return pcol->is_virtual();
}

size_t Column::elemsize() const noexcept {
  return info(pcol->stype_).elemsize();
}

Column::operator bool() const noexcept {
  return (pcol != nullptr);
}




//------------------------------------------------------------------------------
// Column : data accessors
//------------------------------------------------------------------------------

bool Column::get_element(size_t i, int8_t* out) const {
  xassert(i < nrows());
  return pcol->get_element(i, out);
}

bool Column::get_element(size_t i, int16_t* out) const {
  xassert(i < nrows());
  return pcol->get_element(i, out);
}

bool Column::get_element(size_t i, int32_t* out) const {
  xassert(i < nrows());
  return pcol->get_element(i, out);
}

bool Column::get_element(size_t i, int64_t* out) const {
  xassert(i < nrows());
  return pcol->get_element(i, out);
}

bool Column::get_element(size_t i, float* out) const {
  xassert(i < nrows());
  return pcol->get_element(i, out);
}

bool Column::get_element(size_t i, double* out) const {
  xassert(i < nrows());
  return pcol->get_element(i, out);
}

bool Column::get_element(size_t i, CString* out) const {
  xassert(i < nrows());
  return pcol->get_element(i, out);
}

bool Column::get_element(size_t i, py::robj* out) const {
  xassert(i < nrows());
  return pcol->get_element(i, out);
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
      int32_t x;
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
    default:
      throw NotImplError() << "Unable to convert elements of stype "
          << stype() << " into python objects";
  }
}



//------------------------------------------------------------------------------
// Column : data buffers
//------------------------------------------------------------------------------

bool Column::is_data_editable(size_t k) const {
  if (k != 0) return false;
  return pcol->mbuf.is_writable();
}

const void* Column::get_data_readonly(size_t k) const {
  if (is_virtual()) materialize();
  return k == 0 ? pcol->mbuf.rptr()
                : pcol->data2();
}

void* Column::get_data_editable(size_t) {
  if (is_virtual()) materialize();
  return pcol->mbuf.wptr();
}

Buffer Column::get_data_buffer(size_t) const {
  if (is_virtual()) materialize();
  return pcol->mbuf;
}

size_t Column::get_data_size(size_t k) const {
  if (is_virtual()) materialize();
  return k == 0 ? pcol->mbuf.size()
                : pcol->data2_size();
}



//------------------------------------------------------------------------------
// Column : manipulation
//------------------------------------------------------------------------------

void Column::materialize() const {
  auto self = const_cast<Column*>(this);
  self->pcol = self->pcol->materialize();
}

void Column::replace_values(const RowIndex& replace_at,
                             const Column& replace_with)
{
  materialize();
  pcol->replace_values(*this, replace_at, replace_with);
}


void Column::repeat(size_t ntimes) {
  pcol->repeat(ntimes, *this);
}

void Column::apply_rowindex(const RowIndex& ri) {
  if (!ri) return;
  pcol->apply_rowindex(ri, *this);  // modifies in-place
}

void Column::resize(size_t new_nrows) {
  size_t curr_nrows = nrows();
  if (new_nrows > curr_nrows) pcol->na_pad(new_nrows, *this);
  if (new_nrows < curr_nrows) pcol->truncate(new_nrows, *this);
}


void Column::sort_grouped(const Groupby& grps) {
  pcol->sort_grouped(grps, *this);
}


void Column::verify_integrity() const {
  pcol->verify_integrity();
}
