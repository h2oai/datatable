//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <cstdlib>     // atoll
#include "python/_all.h"
#include "python/string.h"
#include "utils/assert.h"
#include "utils/file.h"
#include "utils/misc.h"
#include "column.h"
#include "datatablemodule.h"
#include "rowindex.h"
#include "sort.h"


Column::Column(size_t nrows)
    : _nrows(nrows) {}

Column::~Column() {}



static Column* new_column_impl(SType stype) {
  switch (stype) {
    case SType::VOID:    return new VoidColumn();
    case SType::BOOL:    return new BoolColumn();
    case SType::INT8:    return new IntColumn<int8_t>();
    case SType::INT16:   return new IntColumn<int16_t>();
    case SType::INT32:   return new IntColumn<int32_t>();
    case SType::INT64:   return new IntColumn<int64_t>();
    case SType::FLOAT32: return new RealColumn<float>();
    case SType::FLOAT64: return new RealColumn<double>();
    case SType::STR32:   return new StringColumn<uint32_t>();
    case SType::STR64:   return new StringColumn<uint64_t>();
    case SType::OBJ:     return new PyObjectColumn();
    default:
      throw ValueError()
          << "Unable to create a column of stype `" << stype << "`";
  }
}


OColumn OColumn::new_data_column(SType stype, size_t nrows) {
  Column* col = new_column_impl(stype);
  col->_nrows = nrows;
  col->init_data();
  return OColumn(col);
}


// TODO: create a special "NA" column instead
OColumn OColumn::new_na_column(SType stype, size_t nrows) {
  OColumn col = OColumn::new_data_column(stype, nrows);
  col->fill_na();
  return col;
}


OColumn OColumn::new_mbuf_column(SType stype, MemoryRange&& mbuf) {
  size_t elemsize = info(stype).elemsize();
  Column* col = new_column_impl(stype);
  xassert(mbuf.size() % elemsize == 0);
  if (stype == SType::OBJ) {
    xassert(mbuf.is_pyobjects() || !mbuf.is_writable());
  }
  col->_nrows = mbuf.size() / elemsize;
  col->mbuf = std::move(mbuf);
  return OColumn(col);
}


bool Column::get_element(size_t, int32_t*) const {
  throw NotImplError()
    << "Cannot retrieve int32 values from a column of type " << _stype;
}

bool Column::get_element(size_t, int64_t*) const {
  throw NotImplError()
    << "Cannot retrieve int64 values from a column of type " << _stype;
}

bool Column::get_element(size_t, float*) const {
  throw NotImplError()
    << "Cannot retrieve float values from a column of type " << _stype;
}

bool Column::get_element(size_t, double*) const {
  throw NotImplError()
    << "Cannot retrieve double values from a column of type " << _stype;
}

bool Column::get_element(size_t, CString*) const {
  throw NotImplError()
    << "Cannot retrieve string values from a column of type " << _stype;
}

bool Column::get_element(size_t, py::robj*) const {
  throw NotImplError()
    << "Cannot retrieve object values from a column of type " << _stype;
}




/**
 * Create a shallow copy of the column; possibly applying the provided rowindex.
 */
Column* Column::shallowcopy() const {
  Column* col = new_column_impl(_stype);
  col->_nrows = _nrows;
  col->mbuf = mbuf;
  col->ri = ri;
  // TODO: also copy Stats object
  return col;
}


size_t Column::alloc_size() const {
  return mbuf.size();
}

PyObject* Column::mbuf_repr() const {
  return mbuf.pyrepr();
}



RowIndex Column::remove_rowindex() {
  RowIndex res(std::move(ri));
  xassert(!ri);
  return res;
}

void Column::replace_rowindex(const RowIndex& newri) {
  ri = newri;
  _nrows = ri.size();
}




//------------------------------------------------------------------------------
// OColumn
//------------------------------------------------------------------------------

void swap(OColumn& lhs, OColumn& rhs) {
  std::swap(lhs.pcol, rhs.pcol);
}

OColumn::OColumn() : pcol(nullptr) {}

OColumn::OColumn(Column* col) : pcol(col) {}  // Steal ownership

OColumn::OColumn(const OColumn& other) : pcol(other.pcol->shallowcopy()) {}

OColumn::OColumn(OColumn&& other) : OColumn() {
  std::swap(pcol, other.pcol);
}

OColumn& OColumn::operator=(const OColumn& other) {
  delete pcol;
  pcol = other.pcol->shallowcopy();
  return *this;
}

OColumn& OColumn::operator=(OColumn&& other) {
  delete pcol;
  pcol = other.pcol;
  other.pcol = nullptr;
  return *this;
}

OColumn::~OColumn() {
  delete pcol;
}



const Column* OColumn::get() const {
  return pcol;  // borrowed ref
}

Column* OColumn::operator->() {
  return pcol;
}

const Column* OColumn::operator->() const {
  return pcol;
}


//---- Properties ----------------------

size_t OColumn::nrows() const noexcept {
  return pcol->_nrows;
}

size_t OColumn::na_count() const {
  return stats()->nacount();
}

SType OColumn::stype() const noexcept {
  return pcol->_stype;
}

LType OColumn::ltype() const noexcept {
  return info(pcol->_stype).ltype();
}

bool OColumn::is_fixedwidth() const noexcept {
  return !info(pcol->_stype).is_varwidth();
}

bool OColumn::is_virtual() const noexcept {
  return bool(pcol->ri);
}

size_t OColumn::elemsize() const noexcept {
  return info(pcol->_stype).elemsize();
}

OColumn::operator bool() const noexcept {
  return (pcol != nullptr);
}


//------------------------------------------------------------------------------
// OColumn : data accessors
//------------------------------------------------------------------------------

bool OColumn::get_element(size_t i, int32_t*  out) const { return pcol->get_element(i, out); }
bool OColumn::get_element(size_t i, int64_t*  out) const { return pcol->get_element(i, out); }
bool OColumn::get_element(size_t i, float*    out) const { return pcol->get_element(i, out); }
bool OColumn::get_element(size_t i, double*   out) const { return pcol->get_element(i, out); }
bool OColumn::get_element(size_t i, CString*  out) const { return pcol->get_element(i, out); }
bool OColumn::get_element(size_t i, py::robj* out) const { return pcol->get_element(i, out); }


static inline py::oobj wrap(int32_t x) { return py::oint(x); }
static inline py::oobj wrap(int64_t x) { return py::oint(x); }
static inline py::oobj wrap(float x) { return py::ofloat(x); }
static inline py::oobj wrap(double x) { return py::ofloat(x); }
static inline py::oobj wrap(const CString& x) { return py::ostring(x); }
static inline py::oobj wrap(const py::robj& x) { return py::oobj(x); }

template <typename T>
static inline py::oobj getelem(const OColumn& col, size_t i) {
  T x;
  bool r = col.get_element(i, &x);
  return r? py::None() : wrap(x);
}

py::oobj OColumn::get_element_as_pyobject(size_t i) const {
  switch (stype()) {
    case SType::BOOL: {
      int32_t x;
      bool r = get_element(i, &x);
      return r? py::None() : py::obool(x);
    }
    case SType::INT8:
    case SType::INT16:
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






void OColumn::materialize() {
  pcol->materialize();
}

void OColumn::replace_values(const RowIndex& replace_at,
                             const OColumn& replace_with)
{
  pcol->replace_values(*this, replace_at, replace_with);
}




//==============================================================================
// VoidColumn
//==============================================================================

VoidColumn::VoidColumn() { _stype = SType::VOID; }
VoidColumn::VoidColumn(size_t nrows) : Column(nrows) { _stype = SType::VOID; }
size_t VoidColumn::data_nrows() const { return _nrows; }
void VoidColumn::materialize() {}
void VoidColumn::resize_and_fill(size_t) {}
void VoidColumn::rbind_impl(colvec&, size_t, bool) {}
void VoidColumn::apply_na_mask(const OColumn&) {}
void VoidColumn::replace_values(OColumn&, const RowIndex&, const OColumn&) {}
void VoidColumn::init_data() {}
void VoidColumn::fill_na() {}
RowIndex VoidColumn::join(const OColumn&) const { return RowIndex(); }
void VoidColumn::fill_na_mask(int8_t*, size_t, size_t) {}




//==============================================================================
// StrvecColumn
//==============================================================================

// Note: this class stores strvec by reference; therefore the lifetime
// of the column may not exceed the lifetime of the string vector.
//
class StrvecColumn : public Column {
  private:
    const strvec& vec;

  public:
    StrvecColumn(const strvec& v);
    bool get_element(size_t i, CString* out) const override;
    Column* shallowcopy() const override;

    size_t data_nrows() const override { return _nrows; }
    void materialize() override {}
    void resize_and_fill(size_t) override {}
    void rbind_impl(colvec&, size_t, bool) override {}
    void apply_na_mask(const OColumn&) override {}
    void replace_values(OColumn&, const RowIndex&, const OColumn&) override {}
    void init_data() override {}
    void fill_na() override {}
    RowIndex join(const OColumn&) const override { return RowIndex(); }
    void fill_na_mask(int8_t*, size_t, size_t) override {}
};

StrvecColumn::StrvecColumn(const strvec& v)
  : Column(v.size()), vec(v)
{
  _stype = SType::STR32;
}

bool StrvecColumn::get_element(size_t i, CString* out) const {
  out->ch = vec[i].c_str();
  out->size = static_cast<int64_t>(vec[i].size());
  return false;
}

Column* StrvecColumn::shallowcopy() const {
  return new StrvecColumn(vec);
}

OColumn OColumn::from_strvec(const strvec& vec) {
  return OColumn(new StrvecColumn(vec));
}
