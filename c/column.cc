//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <cstdlib>     // atoll
#include "column.h"
#include "datatablemodule.h"
#include "py_utils.h"
#include "rowindex.h"
#include "sort.h"
#include "utils.h"
#include "utils/assert.h"
#include "utils/file.h"


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


Column* Column::new_data_column(SType stype, size_t nrows) {
  xassert(stype != SType::VOID);
  Column* col = new_column(stype);
  col->nrows = nrows;
  col->init_data();
  return col;
}

Column* Column::new_na_column(SType stype, size_t nrows) {
  Column* col = new_data_column(stype, nrows);
  col->fill_na();
  return col;
}


Column* Column::new_mmap_column(SType stype, size_t nrows,
                                const std::string& filename) {
  Column* col = new_column(stype);
  col->nrows = nrows;
  col->init_mmap(filename);
  return col;
}



/**
 * Save this column's data buffer into file `filename`. Other information
 * about the column should be stored elsewhere (for example in the _meta.nff
 * file).
 * If a file with the given name already exists, it will be overwritten.
 */
void Column::save_to_disk(const std::string& filename,
                          WritableBuffer::Strategy strategy) {
  mbuf.save_to_disk(filename, strategy);
}



/**
 * Restore a Column previously saved via `column_save_to_disk()`. The column's
 * data buffer is taken from the file `filename`; and the column is assumed to
 * have type `stype`, number of rows `nrows`.
 * This function will not check data validity (i.e. that the buffer contains
 * valid values, and that the extra parameters match the buffer's contents).
 */
Column* Column::open_mmap_column(SType stype, size_t nrows,
                                 const std::string& filename, bool recode)
{
  Column* col = new_column(stype);
  col->nrows = nrows;
  col->open_mmap(filename, recode);
  return col;
}


/**
 * Construct a column from the externally provided buffer.
 */
Column* Column::new_xbuf_column(SType stype,
                                size_t nrows,
                                Py_buffer* pybuffer)
{
  Column* col = new_column(stype);
  col->nrows = nrows;
  col->init_xbuf(pybuffer);
  return col;
}


/**
 * Construct a column using existing MemoryRanges.
 */
Column* Column::new_mbuf_column(SType stype, MemoryRange&& mbuf) {
  Column* col = new_column(stype);
  xassert(mbuf.size() % col->elemsize() == 0);
  xassert(stype == SType::OBJ? mbuf.is_pyobjects() : true);
  col->nrows = mbuf.size() / col->elemsize();
  col->mbuf = std::move(mbuf);
  return col;
}


/**
 * Create a shallow copy of the column; possibly applying the provided rowindex.
 */
Column* Column::shallowcopy(const RowIndex& new_rowindex) const {
  Column* col = new_column(stype());
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
// Integrity checks
//------------------------------------------------------------------------------

void Column::verify_integrity(const std::string& name) const {
  mbuf.verify_integrity();
  ri.verify_integrity();

  size_t mbuf_nrows = data_nrows();

  // Check RowIndex
  if (ri.isabsent()) {
    // Check that nrows is a correct representation of mbuf's size
    if (nrows != mbuf_nrows) {
      throw AssertionError()
          << "Mismatch between reported number of rows: " << name
          << " has nrows=" << nrows << " but MemoryRange has data for "
          << mbuf_nrows << " rows";
    }
  }
  else {
    // Check that the length of the RowIndex corresponds to `nrows`
    if (nrows != ri.size()) {
      throw AssertionError()
          << "Mismatch in reported number of rows: " << name << " has "
          << "nrows=" << nrows << ", while its rowindex.length="
          << ri.size();
    }
    // Check that the maximum value of the RowIndex does not exceed the maximum
    // row number in the memory buffer
    if (ri.max() >= mbuf_nrows && ri.max() != RowIndex::NA) {
      throw AssertionError()
          << "Maximum row number in the rowindex of " << name << " exceeds the "
          << "number of rows in the underlying memory buffer: max(rowindex)="
          << ri.max() << ", and nrows(membuf)=" << mbuf_nrows;
    }
  }

  // Check Stats
  if (stats) { // Stats are allowed to be null
    stats->verify_integrity(this);
  }
}



//==============================================================================
// VoidColumn
//==============================================================================

VoidColumn::VoidColumn() {}
VoidColumn::VoidColumn(size_t nrows) : Column(nrows) {}
SType VoidColumn::stype() const noexcept { return SType::VOID; }
size_t VoidColumn::elemsize() const { return 0; }
bool VoidColumn::is_fixedwidth() const { return true; }
size_t VoidColumn::data_nrows() const { return nrows; }
void VoidColumn::materialize() {}
void VoidColumn::resize_and_fill(size_t) {}
void VoidColumn::rbind_impl(std::vector<const Column*>&, size_t, bool) {}
void VoidColumn::apply_na_mask(const BoolColumn*) {}
void VoidColumn::replace_values(RowIndex, const Column*) {}
void VoidColumn::init_data() {}
void VoidColumn::init_mmap(const std::string&) {}
void VoidColumn::open_mmap(const std::string&, bool) {}
void VoidColumn::init_xbuf(Py_buffer*) {}
Stats* VoidColumn::get_stats() const { return nullptr; }
void VoidColumn::fill_na() {}
RowIndex VoidColumn::join(const Column*) const { return RowIndex(); }
py::oobj VoidColumn::get_value_at_index(size_t) const { return py::oobj(); }
void VoidColumn::fill_na_mask(int8_t*, size_t, size_t) {}
