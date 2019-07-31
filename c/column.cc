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


Column::Column(size_t nrows_)
    : stats(nullptr),
      nrows(nrows_)
{
  TRACK(this, sizeof(*this), "Column");
}

Column::~Column() {
  delete stats;
  UNTRACK(this);
}



Column* Column::new_column(SType stype) {
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
      throw ValueError() << "Unable to create a column of SType = " << stype;
  }
}


OColumn OColumn::new_data_column(SType stype, size_t nrows) {
  Column* col = Column::new_column(stype);
  col->nrows = nrows;
  col->init_data();
  return OColumn(col);
}


// TODO: create a special "NA" column instead
OColumn OColumn::new_na_column(SType stype, size_t nrows) {
  OColumn col = OColumn::new_data_column(stype, nrows);
  col->fill_na();
  return col;
}



/**
 * Construct a column using existing MemoryRanges.
 */
OColumn OColumn::new_mbuf_column(SType stype, MemoryRange&& mbuf) {
  size_t elemsize = info(stype).elemsize();
  Column* col = Column::new_column(stype);
  xassert(mbuf.size() % elemsize == 0);
  if (stype == SType::OBJ) {
    xassert(mbuf.is_pyobjects() || !mbuf.is_writable());
  }
  col->nrows = mbuf.size() / elemsize;
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

bool Column::get_element(size_t, py::oobj*) const {
  throw NotImplError()
    << "Cannot retrieve object values from a column of type " << _stype;
}




/**
 * Create a shallow copy of the column; possibly applying the provided rowindex.
 */
Column* Column::shallowcopy(const RowIndex& new_rowindex) const {
  Column* col = new_column(_stype);
  col->nrows = nrows;
  col->mbuf = mbuf;
  // TODO: also copy Stats object

  if (new_rowindex) {
    col->ri = new_rowindex;
    col->nrows = new_rowindex.size();
  } else if (ri) {
    col->ri = ri;
  }
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
  nrows = ri.size();
}



//------------------------------------------------------------------------------
// Stats
//------------------------------------------------------------------------------

size_t Column::countna() const { return get_stats()->countna(this); }
size_t Column::nunique() const { return get_stats()->nunique(this); }
size_t Column::nmodal() const  { return get_stats()->nmodal(this); }



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
  return pcol->nrows;
}

size_t OColumn::na_count() const {
  return pcol->countna();
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
  return bool(pcol->rowindex());
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
bool OColumn::get_element(size_t i, py::oobj* out) const { return pcol->get_element(i, out); }


static inline py::oobj wrap(int32_t x) { return py::oint(x); }
static inline py::oobj wrap(int64_t x) { return py::oint(x); }
static inline py::oobj wrap(float x) { return py::ofloat(x); }
static inline py::oobj wrap(double x) { return py::ofloat(x); }
static inline py::oobj wrap(const CString& x) { return py::ostring(x); }
static inline py::oobj wrap(const py::oobj& x) { return x; }

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
    case SType::OBJ:     return getelem<py::oobj>(*this, i);
    default:
      throw NotImplError() << "Unable to convert elements of stype "
          << stype() << " into python objects";
  }
}






void OColumn::materialize() {
  pcol->materialize();
}

OColumn OColumn::cast(SType stype) const {
  return OColumn(pcol->cast(stype));
}

OColumn OColumn::cast(SType stype, MemoryRange&& mem) const {
  return OColumn(pcol->cast(stype, std::move(mem)));
}




//==============================================================================
// VoidColumn
//==============================================================================

VoidColumn::VoidColumn() { _stype = SType::VOID; }
VoidColumn::VoidColumn(size_t nrows) : Column(nrows) { _stype = SType::VOID; }
size_t VoidColumn::data_nrows() const { return nrows; }
void VoidColumn::materialize() {}
void VoidColumn::resize_and_fill(size_t) {}
void VoidColumn::rbind_impl(colvec&, size_t, bool) {}
void VoidColumn::apply_na_mask(const BoolColumn*) {}
void VoidColumn::replace_values(RowIndex, const OColumn&) {}
void VoidColumn::init_data() {}
Stats* VoidColumn::get_stats() const { return nullptr; }
void VoidColumn::fill_na() {}
RowIndex VoidColumn::join(const Column*) const { return RowIndex(); }
void VoidColumn::fill_na_mask(int8_t*, size_t, size_t) {}
