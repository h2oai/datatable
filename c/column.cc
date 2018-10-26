//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "column.h"
#include <cstdlib>     // atoll
#include "py_utils.h"
#include "rowindex.h"
#include "sort.h"
#include "utils.h"
#include "utils/assert.h"
#include "utils/file.h"


Column::Column(size_t nrows_)
    : stats(nullptr),
      nrows(nrows_) {}


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
  col->replace_buffer(std::move(mbuf));
  return col;
}

Column* Column::new_mbuf_column(SType stype, MemoryRange&& mbuf,
                                MemoryRange&& strbuf)
{
  Column* col = new_column(stype);
  if (stype == SType::STR32 || stype == SType::STR64) {
    col->replace_buffer(std::move(mbuf), std::move(strbuf));
  } else {
    xassert(!strbuf);
    col->replace_buffer(std::move(mbuf));
  }
  return col;
}



void Column::replace_buffer(MemoryRange&&) {
  throw RuntimeError()
    << "replace_buffer(mr) not valid for Column of type " << stype();
}

void Column::replace_buffer(MemoryRange&&, MemoryRange&&) {
  throw RuntimeError()
    << "replace_buffer(mr1, mr2) not valid for Column of type " << stype();
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
    col->nrows = new_rowindex.length();
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



Column* Column::rbind(std::vector<const Column*>& columns)
{
  // Is the current column "empty" ?
  bool col_empty = (stype() == SType::VOID);
  // Compute the final number of rows and stype
  size_t new_nrows = this->nrows;
  SType new_stype = col_empty? SType::BOOL : stype();
  for (const Column* col : columns) {
    new_nrows += col->nrows;
    new_stype = std::max(new_stype, col->stype());
  }

  // Create the resulting Column object. It can be either: an empty column
  // filled with NAs; the current column (`this`); a clone of the current
  // column (if it has refcount > 1); or a type-cast of the current column.
  Column* res = nullptr;
  if (col_empty) {
    res = Column::new_na_column(new_stype, this->nrows);
  } else if (stype() == new_stype) {
    res = this;
  } else {
    res = this->cast(new_stype);
  }
  xassert(res->stype() == new_stype);

  // TODO: Temporary Fix. To be resolved in #301
  if (res->stats != nullptr) res->stats->reset();

  // Use the appropriate strategy to continue appending the columns.
  res->rbind_impl(columns, new_nrows, col_empty);

  // If everything is fine, then the current column can be safely discarded
  // -- the upstream caller will replace this column with the `res`.
  if (res != this) delete this;
  return res;
}


void Column::replace_rowindex(const RowIndex& newri) {
  ri = newri;
  nrows = ri.length();
}



Column::~Column() {
  delete stats;
}


/**
 * Get the total size of the memory occupied by this Column. This is different
 * from `column->alloc_size`, which in general reports byte size of the `data`
 * portion of the column.
 */
size_t Column::memory_footprint() const
{
  size_t sz = sizeof(*this);
  sz += mbuf.memory_footprint();
  // sz += ri.memory_footprint();
  if (stats) sz += stats->memory_footprint();
  return sz;
}



//------------------------------------------------------------------------------
// Stats
//------------------------------------------------------------------------------

int64_t Column::countna() const { return get_stats()->countna(this); }
int64_t Column::nunique() const { return get_stats()->nunique(this); }
int64_t Column::nmodal() const  { return get_stats()->nmodal(this); }


/**
 * Methods for retrieving stats but in column form. These should be populated
 * with NA values when called from the base column instance.
 */
Column* Column::mean_column() const { return new_na_column(SType::FLOAT64, 1); }
Column* Column::sd_column() const   { return new_na_column(SType::FLOAT64, 1); }
Column* Column::skew_column() const { return new_na_column(SType::FLOAT64, 1); }
Column* Column::kurt_column() const { return new_na_column(SType::FLOAT64, 1); }
Column* Column::min_column() const  { return new_na_column(stype(), 1); }
Column* Column::max_column() const  { return new_na_column(stype(), 1); }
Column* Column::mode_column() const { throw NotImplError(); }
Column* Column::sum_column() const  { return new_na_column(stype(), 1); }

Column* Column::countna_column() const {
  IntColumn<int64_t>* col = new IntColumn<int64_t>(1);
  col->set_elem(0, countna());
  return col;
}

Column* Column::nunique_column() const {
  IntColumn<int64_t>* col = new IntColumn<int64_t>(1);
  col->set_elem(0, nunique());
  return col;
}

Column* Column::nmodal_column() const {
  IntColumn<int64_t>* col = new IntColumn<int64_t>(1);
  col->set_elem(0, nmodal());
  return col;
}

PyObject* Column::mean_pyscalar() const { return none(); }
PyObject* Column::sd_pyscalar() const { return none(); }
PyObject* Column::skew_pyscalar() const { return none(); }
PyObject* Column::kurt_pyscalar() const { return none(); }
PyObject* Column::min_pyscalar() const { return none(); }
PyObject* Column::max_pyscalar() const { return none(); }
PyObject* Column::mode_pyscalar() const { throw NotImplError(); }
PyObject* Column::sum_pyscalar() const { return none(); }
PyObject* Column::countna_pyscalar() const { return int_to_py(countna()); }
PyObject* Column::nunique_pyscalar() const { return int_to_py(nunique()); }
PyObject* Column::nmodal_pyscalar() const { return int_to_py(nmodal()); }



//------------------------------------------------------------------------------
// Casting
//------------------------------------------------------------------------------

Column* Column::cast(SType new_stype) const {
  return cast(new_stype, MemoryRange());
}

Column* Column::cast(SType new_stype, MemoryRange&& mr) const {
  if (new_stype == stype()) {
    return shallowcopy();
  }
  if (ri) {
    // TODO: implement this
    throw RuntimeError() << "Cannot cast a column with rowindex";
  }
  Column *res = nullptr;
  if (mr) {
    res = Column::new_column(new_stype);
    res->nrows = nrows;
    res->mbuf = std::move(mr);
  } else {
    res = Column::new_data_column(new_stype, nrows);
  }
  switch (new_stype) {
    case SType::BOOL:    cast_into(static_cast<BoolColumn*>(res)); break;
    case SType::INT8:    cast_into(static_cast<IntColumn<int8_t>*>(res)); break;
    case SType::INT16:   cast_into(static_cast<IntColumn<int16_t>*>(res)); break;
    case SType::INT32:   cast_into(static_cast<IntColumn<int32_t>*>(res)); break;
    case SType::INT64:   cast_into(static_cast<IntColumn<int64_t>*>(res)); break;
    case SType::FLOAT32: cast_into(static_cast<RealColumn<float>*>(res)); break;
    case SType::FLOAT64: cast_into(static_cast<RealColumn<double>*>(res)); break;
    case SType::STR32:   cast_into(static_cast<StringColumn<uint32_t>*>(res)); break;
    case SType::STR64:   cast_into(static_cast<StringColumn<uint64_t>*>(res)); break;
    case SType::OBJ:     cast_into(static_cast<PyObjectColumn*>(res)); break;
    default:
      throw ValueError() << "Unable to cast into stype = " << new_stype;
  }
  return res;
}

void Column::cast_into(BoolColumn*) const {
  throw ValueError() << "Cannot cast " << stype() << " into bool";
}
void Column::cast_into(IntColumn<int8_t>*) const {
  throw ValueError() << "Cannot cast " << stype() << " into int8";
}
void Column::cast_into(IntColumn<int16_t>*) const {
  throw ValueError() << "Cannot cast " << stype() << " into int16";
}
void Column::cast_into(IntColumn<int32_t>*) const {
  throw ValueError() << "Cannot cast " << stype() << " into int32";
}
void Column::cast_into(IntColumn<int64_t>*) const {
  throw ValueError() << "Cannot cast " << stype() << " into int64";
}
void Column::cast_into(RealColumn<float>*) const {
  throw ValueError() << "Cannot cast " << stype() << " into float";
}
void Column::cast_into(RealColumn<double>*) const {
  throw ValueError() << "Cannot cast " << stype() << " into double";
}
void Column::cast_into(StringColumn<uint32_t>*) const {
  throw ValueError() << "Cannot cast " << stype() << " into str32";
}
void Column::cast_into(StringColumn<uint64_t>*) const {
  throw ValueError() << "Cannot cast " << stype() << " into str64";
}
void Column::cast_into(PyObjectColumn*) const {
  throw ValueError() << "Cannot cast " << stype() << " into pyobj";
}



//------------------------------------------------------------------------------
// Integrity checks
//------------------------------------------------------------------------------

void Column::verify_integrity(const std::string& name) const {
  if (nrows < 0) {
    throw AssertionError()
      << name << " has a negative value for nrows: " << nrows;
  }
  mbuf.verify_integrity();
  ri.verify_integrity();

  int64_t mbuf_nrows = data_nrows();

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
    if (nrows != ri.length()) {
      throw AssertionError()
          << "Mismatch in reported number of rows: " << name << " has "
          << "nrows=" << nrows << ", while its rowindex.length="
          << ri.length();
    }
    // Check that the maximum value of the RowIndex does not exceed the maximum
    // row number in the memory buffer
    if (ri.max() >= mbuf_nrows && ri.max() > 0) {
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
VoidColumn::VoidColumn(int64_t nrows) : Column(nrows) {}
SType VoidColumn::stype() const { return SType::VOID; }
size_t VoidColumn::elemsize() const { return 0; }
bool VoidColumn::is_fixedwidth() const { return true; }
size_t VoidColumn::data_nrows() const { return nrows; }
void VoidColumn::reify() {}
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
py::oobj VoidColumn::get_value_at_index(int64_t) const { return py::oobj(); }
