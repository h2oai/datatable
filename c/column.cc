//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#include <cstdlib>     // atoll
#include "column/const.h"
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






Column Column::new_data_column(SType stype, size_t nrows) {
  ColumnImpl* col = ColumnImpl::new_impl(stype);
  col->_nrows = nrows;
  col->init_data();
  return Column(col);
}


Column Column::new_na_column(SType stype, size_t nrows) {
  return Column(new dt::ConstNa_ColumnImpl(nrows, stype));
}


Column Column::new_mbuf_column(SType stype, Buffer&& mbuf) {
  size_t elemsize = info(stype).elemsize();
  ColumnImpl* col = ColumnImpl::new_impl(stype);
  xassert(mbuf.size() % elemsize == 0);
  if (stype == SType::OBJ) {
    xassert(mbuf.is_pyobjects() || !mbuf.is_writable());
  }
  col->_nrows = mbuf.size() / elemsize;
  col->mbuf = std::move(mbuf);
  return Column(col);
}


static Buffer _recode_offsets_to_u64(const Buffer& offsets) {
  // TODO: make this parallel
  Buffer off64 = Buffer::mem(offsets.size() * 2);
  auto data64 = static_cast<uint64_t*>(off64.xptr());
  auto data32 = static_cast<const uint32_t*>(offsets.rptr());
  data64[0] = 0;
  uint64_t curr_offset = 0;
  size_t n = offsets.size() / sizeof(uint32_t) - 1;
  for (size_t i = 1; i <= n; ++i) {
    uint32_t len = data32[i] - data32[i - 1];
    if (len == GETNA<uint32_t>()) {
      data64[i] = curr_offset ^ GETNA<uint64_t>();
    } else {
      curr_offset += len & ~GETNA<uint32_t>();
      data64[i] = curr_offset;
    }
  }
  return off64;
}


Column Column::new_string_column(
    size_t n, Buffer&& data, Buffer&& str)
{
  size_t data_size = data.size();
  size_t strb_size = str.size();

  if (data_size == sizeof(uint32_t) * (n + 1)) {
    if (strb_size <= Column::MAX_ARR32_SIZE &&
        n <= Column::MAX_ARR32_SIZE) {
      return Column(new StringColumn<uint32_t>(n, std::move(data), std::move(str)));
    }
    // Otherwise, offsets need to be recoded into a uint64_t array
    data = _recode_offsets_to_u64(data);
  }
  return Column(new StringColumn<uint64_t>(n, std::move(data), std::move(str)));
}





//------------------------------------------------------------------------------
// Column
//------------------------------------------------------------------------------

void swap(Column& lhs, Column& rhs) {
  std::swap(lhs.pcol, rhs.pcol);
}

Column::Column()
  : pcol(nullptr) {}

Column::Column(ColumnImpl* col)
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
  return pcol->_nrows;
}

size_t Column::na_count() const {
  return stats()->nacount();
}

SType Column::stype() const noexcept {
  return pcol->_stype;
}

LType Column::ltype() const noexcept {
  return info(pcol->_stype).ltype();
}

bool Column::is_fixedwidth() const noexcept {
  return !info(pcol->_stype).is_varwidth();
}

bool Column::is_virtual() const noexcept {
  return pcol->is_virtual();
}

size_t Column::elemsize() const noexcept {
  return info(pcol->_stype).elemsize();
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


const void* Column::get_data_readonly(size_t k) const {
  if (is_virtual()) materialize();
  return k == 0 ? pcol->mbuf.rptr()
                : pcol->data2();
}

void* Column::get_data_editable(size_t) {
  if (is_virtual()) materialize();
  return pcol->mbuf.wptr();
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



//==============================================================================
// VoidColumn
//==============================================================================

VoidColumn::VoidColumn() : ColumnImpl(0, SType::VOID) {}
VoidColumn::VoidColumn(size_t nrows) : ColumnImpl(nrows, SType::VOID) {}
size_t VoidColumn::data_nrows() const { return _nrows; }
ColumnImpl* VoidColumn::materialize() { return this; }
void VoidColumn::apply_na_mask(const Column&) {}
void VoidColumn::replace_values(Column&, const RowIndex&, const Column&) {}
void VoidColumn::init_data() {}




//==============================================================================
// StrvecColumn
//==============================================================================

// Note: this class stores strvec by reference; therefore the lifetime
// of the column may not exceed the lifetime of the string vector.
//
class StrvecColumn : public ColumnImpl {
  private:
    const strvec& vec;

  public:
    StrvecColumn(const strvec& v);
    bool get_element(size_t i, CString* out) const override;
    ColumnImpl* shallowcopy() const override;

    size_t data_nrows() const override { return _nrows; }
    ColumnImpl* materialize() override { return this; }
    void apply_na_mask(const Column&) override {}
    void replace_values(Column&, const RowIndex&, const Column&) override {}
    void init_data() override {}
};

StrvecColumn::StrvecColumn(const strvec& v)
  : ColumnImpl(v.size(), SType::STR32), vec(v) {}

bool StrvecColumn::get_element(size_t i, CString* out) const {
  out->ch = vec[i].c_str();
  out->size = static_cast<int64_t>(vec[i].size());
  return true;
}

ColumnImpl* StrvecColumn::shallowcopy() const {
  return new StrvecColumn(vec);
}

Column Column::from_strvec(const strvec& vec) {
  return Column(new StrvecColumn(vec));
}
