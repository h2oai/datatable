//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "column.h"
#include <cstdlib>     // atoll
#include "datatable_check.h"
#include "py_utils.h"
#include "rowindex.h"
#include "sort.h"
#include "utils.h"
#include "utils/assert.h"
#include "utils/file.h"


Column::Column(int64_t nrows_)
    : mbuf(nullptr),
      stats(nullptr),
      nrows(nrows_) {}


Column* Column::new_column(SType stype) {
  switch (stype) {
    case ST_VOID:            return new VoidColumn();
    case ST_BOOLEAN_I1:      return new BoolColumn();
    case ST_INTEGER_I1:      return new IntColumn<int8_t>();
    case ST_INTEGER_I2:      return new IntColumn<int16_t>();
    case ST_INTEGER_I4:      return new IntColumn<int32_t>();
    case ST_INTEGER_I8:      return new IntColumn<int64_t>();
    case ST_REAL_F4:         return new RealColumn<float>();
    case ST_REAL_F8:         return new RealColumn<double>();
    case ST_STRING_I4_VCHAR: return new StringColumn<int32_t>();
    case ST_STRING_I8_VCHAR: return new StringColumn<int64_t>();
    case ST_OBJECT_PYPTR:    return new PyObjectColumn();
    default:
      throw ValueError() << "Unable to create a column of SType = " << stype;
  }
}


Column* Column::new_data_column(SType stype, int64_t nrows) {
  Column* col = new_column(stype);
  col->nrows = nrows;
  col->init_data();
  return col;
}

Column* Column::new_na_column(SType stype, int64_t nrows) {
  Column* col = new_data_column(stype, nrows);
  col->fill_na();
  return col;
}


Column* Column::new_mmap_column(SType stype, int64_t nrows,
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
  assert(mbuf != nullptr);
  mbuf->save_to_disk(filename, strategy);
}



/**
 * Restore a Column previously saved via `column_save_to_disk()`. The column's
 * data buffer is taken from the file `filename`; and the column is assumed to
 * have type `stype`, number of rows `nrows`, and its meta information stored
 * as a string `metastr`.
 * This function will not check data validity (i.e. that the buffer contains
 * valid values, and that the extra parameters match the buffer's contents).
 */
Column* Column::open_mmap_column(SType stype, int64_t nrows,
                                 const std::string& filename)
{
  Column* col = new_column(stype);
  col->nrows = nrows;
  col->open_mmap(filename);
  return col;
}


/**
 * Construct a column from the externally provided buffer.
 */
Column* Column::new_xbuf_column(SType stype,
                                int64_t nrows,
                                Py_buffer* pybuffer)
{
  Column* col = new_column(stype);
  col->nrows = nrows;
  col->init_xbuf(pybuffer);
  return col;
}


/**
 * Construct a column using existing MemoryBuffers.
 */
Column* Column::new_mbuf_column(SType stype, MemoryBuffer* mbuf,
                                MemoryBuffer* strbuf)
{
  Column* col = new_column(stype);
  col->replace_buffer(mbuf, strbuf);
  return col;
}


/**
 * Create a shallow copy of the column; possibly applying the provided rowindex.
 */
Column* Column::shallowcopy(const RowIndex& new_rowindex) const {
  Column* col = new_column(stype());
  col->nrows = nrows;
  col->mbuf = mbuf->shallowcopy();
  // TODO: also copy Stats object

  if (new_rowindex) {
    col->ri = new_rowindex;
    col->nrows = new_rowindex.length();
  } else if (ri) {
    col->ri = ri;
  }
  return col;
}



/**
 * Make a "deep" copy of the column. The column created with this method will
 * have memory-type MT_DATA and refcount of 1.
 */
Column* Column::deepcopy() const
{
  // TODO: it appears this method is not used anywhere...
  Column* col = new_column(stype());
  col->nrows = nrows;
  col->mbuf = mbuf->deepcopy();
  col->ri = rowindex();  // this is shallow copy. Do we need deep?
  // TODO: deep copy stats when implemented
  return col;
}



size_t Column::alloc_size() const {
  return mbuf->size();
}

PyObject* Column::mbuf_repr() const {
  return mbuf->pyrepr();
}

int Column::mbuf_refcount() const {
  return mbuf->get_refcount();
}

MemoryBuffer* Column::mbuf_shallowcopy() const {
  return mbuf->shallowcopy();
}



Column* Column::rbind(const std::vector<const Column*>& columns)
{
    // Is the current column "empty" ?
    bool col_empty = (stype() == ST_VOID);
    // Compute the final number of rows and stype
    int64_t new_nrows = this->nrows;
    SType new_stype = std::max(stype(), ST_BOOLEAN_I1);
    for (const Column* col : columns) {
        new_nrows += col->nrows;
        new_stype = std::max(new_stype, col->stype());
    }

    // Create the resulting Column object. It can be either: an empty column
    // filled with NAs; the current column (`this`); a clone of the current
    // column (if it has refcount > 1); or a type-cast of the current column.
    Column *res = nullptr;
    if (col_empty) {
        res = Column::new_na_column(new_stype, this->nrows);
    } else if (stype() == new_stype) {
        res = this;
    } else {
        res = this->cast(new_stype);
    }
    assert(res->stype() == new_stype);

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
  if (mbuf) mbuf->release();
}


/**
 * Get the total size of the memory occupied by this Column. This is different
 * from `column->alloc_size`, which in general reports byte size of the `data`
 * portion of the column.
 */
size_t Column::memory_footprint() const
{
  size_t sz = sizeof(*this);
  sz += mbuf->memory_footprint();
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
Column* Column::mean_column() const { return new_na_column(ST_REAL_F8, 1); }
Column* Column::sd_column() const   { return new_na_column(ST_REAL_F8, 1); }
Column* Column::min_column() const  { return new_na_column(stype(), 1); }
Column* Column::max_column() const  { return new_na_column(stype(), 1); }
Column* Column::mode_column() const { return new_na_column(stype(), 1); }
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
PyObject* Column::min_pyscalar() const { return none(); }
PyObject* Column::max_pyscalar() const { return none(); }
PyObject* Column::mode_pyscalar() const { return none(); }
PyObject* Column::sum_pyscalar() const { return none(); }
PyObject* Column::countna_pyscalar() const { return int_to_py(countna()); }
PyObject* Column::nunique_pyscalar() const { return int_to_py(nunique()); }
PyObject* Column::nmodal_pyscalar() const { return int_to_py(nmodal()); }



//------------------------------------------------------------------------------
// Casting
//------------------------------------------------------------------------------

Column* Column::cast(SType new_stype, MemoryBuffer* mb) const {
  if (new_stype == stype()) {
    return shallowcopy();
  }
  if (ri) {
    // TODO: implement this
    throw RuntimeError() << "Cannot cast a column with rowindex";
  }
  Column *res = nullptr;
  if (mb) {
    res = Column::new_column(new_stype);
    res->nrows = nrows;
    res->mbuf = mb;
  } else {
    res = Column::new_data_column(new_stype, nrows);
  }
  switch (new_stype) {
    case ST_BOOLEAN_I1:      cast_into(static_cast<BoolColumn*>(res)); break;
    case ST_INTEGER_I1:      cast_into(static_cast<IntColumn<int8_t>*>(res)); break;
    case ST_INTEGER_I2:      cast_into(static_cast<IntColumn<int16_t>*>(res)); break;
    case ST_INTEGER_I4:      cast_into(static_cast<IntColumn<int32_t>*>(res)); break;
    case ST_INTEGER_I8:      cast_into(static_cast<IntColumn<int64_t>*>(res)); break;
    case ST_REAL_F4:         cast_into(static_cast<RealColumn<float>*>(res)); break;
    case ST_REAL_F8:         cast_into(static_cast<RealColumn<double>*>(res)); break;
    case ST_STRING_I4_VCHAR: cast_into(static_cast<StringColumn<int32_t>*>(res)); break;
    case ST_STRING_I8_VCHAR: cast_into(static_cast<StringColumn<int64_t>*>(res)); break;
    case ST_OBJECT_PYPTR:    cast_into(static_cast<PyObjectColumn*>(res)); break;
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
void Column::cast_into(StringColumn<int32_t>*) const {
  throw ValueError() << "Cannot cast " << stype() << " into str32";
}
void Column::cast_into(StringColumn<int64_t>*) const {
  throw ValueError() << "Cannot cast " << stype() << " into str64";
}
void Column::cast_into(PyObjectColumn*) const {
  throw ValueError() << "Cannot cast " << stype() << " into pyobj";
}



//------------------------------------------------------------------------------
// Integrity checks
//------------------------------------------------------------------------------

bool Column::verify_integrity(IntegrityCheckContext& icc,
                              const std::string& name) const
{
  int nerrors = icc.n_errors();
  auto end = icc.end();

  if (nrows < 0) {
    icc << name << " has a negative value for `nrows`: " <<  nrows << end;
  }
  if (mbuf == nullptr) {
    icc << name << " has a null internal memory buffer" << end;
  } else {
    mbuf->verify_integrity(icc);
  }
  if (icc.has_errors(nerrors)) return false;

  // data_nrows() may use the value in `meta`, so `meta` should be checked
  // before using this method
  int64_t mbuf_nrows = data_nrows();

  // Check RowIndex
  RowIndex col_rz(rowindex());
  if (col_rz.isabsent()) {
    // Check that nrows is a correct representation of mbuf's size
    if (nrows != mbuf_nrows) {
      icc << "Mismatch between reported number of rows: " << name
          << " has nrows=" << nrows << " but MemoryBuffer has data for "
          << mbuf_nrows << " rows" << end;
    }
  }
  else {
    // RowIndex check
    bool ok = col_rz.verify_integrity(icc);
    if (!ok) return false;

    // Check that the length of the RowIndex corresponds to `nrows`
    if (nrows != col_rz.length()) {
      icc << "Mismatch in reported number of rows: " << name << " has "
          << "nrows=" << nrows << ", while its rowindex.length="
          << col_rz.length() << end;
    }

    // Check that the maximum value of the RowIndex does not exceed the maximum
    // row number in the memory buffer
    if (col_rz.max() >= mbuf_nrows && col_rz.max() > 0) {
      icc << "Maximum row number in the rowindex of " << name << " exceeds the "
          << "number of rows in the underlying memory buffer: max(rowindex)="
          << col_rz.max() << ", and nrows(membuf)=" << mbuf_nrows << end;
    }
  }
  if (icc.has_errors(nerrors)) return false;

  // Check Stats
  if (stats != nullptr) { // Stats are allowed to be null
    bool r = stats->verify_integrity(icc);
    if (!r) return false;
  }
  return !icc.has_errors(nerrors);
}
