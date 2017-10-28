//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
#include "column.h"
#include <errno.h>     // errno
#include <sys/mman.h>  // mmap
#include <string.h>    // memcpy, strcmp, strerror
#include <cstdlib>     // atoll
#include "datatable_check.h"
#include "file.h"
#include "myassert.h"
#include "py_utils.h"
#include "rowindex.h"
#include "sort.h"
#include "utils.h"


// TODO: make this function virtual
size_t Column::allocsize0(SType stype, int64_t nrows) {
  size_t sz = static_cast<size_t>(nrows) * stype_info[stype].elemsize;
  if (stype == ST_STRING_I4_VCHAR) sz += i4s_padding(0);
  if (stype == ST_STRING_I8_VCHAR) sz += i8s_padding(0);
  return sz;
}


Column::Column(int64_t nrows_)
    : mbuf(nullptr),
      ri(nullptr),
      stats(nullptr),
      meta(nullptr),
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
  col->mbuf = new MemoryMemBuf(allocsize0(stype, nrows));
  return col;
}

Column* Column::new_na_column(SType stype, int64_t nrows) {
  Column* col = new_data_column(stype, nrows);
  col->fill_na();
  return col;
}


Column* Column::new_mmap_column(SType stype, int64_t nrows,
                                const char* filename) {
  size_t sz = allocsize0(stype, nrows);
  Column* col = new_column(stype);
  col->nrows = nrows;
  col->mbuf = new MemmapMemBuf(filename, sz);
  return col;
}



/**
 * Save this column's data buffer into file `filename`. Other information
 * about the column should be stored elsewhere (for example in the _meta.nff
 * file).
 * If a file with the given name already exists, it will be overwritten.
 */
Column* Column::save_to_disk(const char* filename)
{
  void* mmp = nullptr;
  size_t sz = mbuf->size();
  {
    File file(filename, File::CREATE);
    file.resize(sz);

    mmp = mmap(nullptr, sz, PROT_READ|PROT_WRITE, MAP_SHARED,
               file.descriptor(), 0);
    if (mmp == MAP_FAILED) {
      throw RuntimeError() << "Memory-map failed for file " << filename
                           << ": " << Errno;
    }
  }

  // Copy the data buffer into the file
  memcpy(mmp, data(), sz);
  munmap(mmp, sz);
  return this;
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
                                 const char* filename, const char* ms)
{
  Column* col = new_column(stype);
  col->nrows = nrows;
  col->mbuf = new MemmapMemBuf(filename);
  // if (col->alloc_size() < allocsize0(stype, nrows)) {
  //   throw Error("File %s has size %zu, which is not sufficient for a column"
  //               " with %zd rows", filename, col->alloc_size(), nrows);
  // }
  // Deserialize the meta information, if needed
  if (stype == ST_STRING_I4_VCHAR || stype == ST_STRING_I8_VCHAR) {
    if (strncmp(ms, "offoff=", 7) != 0) {
      throw ValueError() << "Cannot retrieve required metadata in string "
                         << '"' << ms << '"';
    }
    int64_t offoff = (int64_t) atoll(ms + 7);
    ((VarcharMeta*) col->meta)->offoff = offoff;
  }
  return col;
}


/**
 * Construct a column from the externally provided buffer.
 */
Column* Column::new_xbuf_column(SType stype, int64_t nrows, void* pybuffer,
                                void* data, size_t a_size)
{
  Column* col = new_column(stype);
  col->nrows = nrows;
  col->mbuf = new ExternalMemBuf(data, pybuffer, a_size);
  return col;
}



/**
 * Create a shallow copy of the column; possibly applying the provided rowindex.
 */
Column* Column::shallowcopy(RowIndex* new_rowindex) const {
  Column* col = new_column(stype());
  col->nrows = nrows;
  col->mbuf = mbuf->shallowcopy();
  if (meta) {
    memcpy(col->meta, meta, stype_info[stype()].metasize);
  }
  // TODO: also copy Stats object

  if (new_rowindex) {
    col->ri = new_rowindex->shallowcopy();
    col->nrows = new_rowindex->length;
  } else if (ri) {
    col->ri = ri->shallowcopy();
  }
  return col;
}



/**
 * Make a "deep" copy of the column. The column created with this method will
 * have memory-type MT_DATA and refcount of 1.
 */
Column* Column::deepcopy() const
{
  Column* col = new_column(stype());
  col->nrows = nrows;
  col->mbuf = mbuf->deepcopy();
  if (meta) {
    memcpy(col->meta, meta, stype_info[stype()].metasize);
  }
  // TODO: deep copy stats when implemented
  col->ri = rowindex() == nullptr ? nullptr : new RowIndex(rowindex());
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
        // FIXME: this is not filled with NAs!
        res = Column::new_data_column(new_stype, this->nrows);
    } else if (stype() == new_stype) {
        mbuf = mbuf->safe_resize(mbuf->size());  // ensure mbuf is writable
        res = this;
    } else {
        res = this->cast(new_stype);
    }
    assert(res->stype() == new_stype && !res->mbuf->is_readonly());

    // TODO: Temporary Fix. To be resolved in #301
    if (res->stats != nullptr) res->stats->reset();

    // Use the appropriate strategy to continue appending the columns.
    res->rbind_impl(columns, new_nrows, col_empty);

    // If everything is fine, then the current column can be safely discarded
    // -- the upstream caller will replace this column with the `res`.
    if (res != this) delete this;
    return res;
}


Column::~Column() {
  dtfree(meta);
  delete stats;
  if (mbuf) mbuf->release();
  if (ri) ri->release();
}


/**
 * Compute the amount of padding between the data and offset section for an
 * ST_STRING_I4_VCHAR column. The formula ensures that datasize + padding are
 * always 8-byte aligned, and that the amount of padding is at least 4 bytes.
 */
size_t Column::i4s_padding(size_t datasize) {
  return ((8 - ((datasize + 4) & 7)) & 7) + 4;
}
size_t Column::i8s_padding(size_t datasize) {
  return ((8 - (datasize & 7)) & 7) + 8;
}



/**
 * Get the total size of the memory occupied by this Column. This is different
 * from `column->alloc_size`, which in general reports byte size of the `data`
 * portion of the column.
 */
size_t Column::memory_footprint() const
{
  size_t sz = sizeof(Column);
  sz += mbuf->memory_footprint();
  sz += stype_info[stype()].metasize;
  if (rowindex() != nullptr) sz += ri->alloc_size();
  return sz;
}


int64_t Column::countna() const {
  Stats* s = get_stats();
  if (!s->countna_computed()) s->compute_countna(this);
  return s->_countna;
}
/**
 * Methods for retrieving stats but in column form. These should be populated
 * with NA values when called from the base column instance.
 */
Column* Column::mean_column() const    { return new_na_column(ST_REAL_F8, 1); }
Column* Column::sd_column() const      { return new_na_column(ST_REAL_F8, 1); }
Column* Column::min_column() const     { return new_na_column(stype(), 1); }
Column* Column::max_column() const     { return new_na_column(stype(), 1); }
Column* Column::sum_column() const     { return new_na_column(stype(), 1); }

Column* Column::countna_column() const {
  IntColumn<int64_t>* col = new IntColumn<int64_t>(1);
  col->set_elem(0, countna());
  return col;
}

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
  RowIndex* col_ri = rowindex();
  if (col_ri != nullptr) { // rowindexes are allowed to be null
    // RowIndex check
    bool ok = col_ri->verify_integrity(icc);
    if (!ok) return false;

    // Check that the length of the RowIndex corresponds to `nrows`
    if (nrows != col_ri->length) {
      icc << "Mismatch in reported number of rows: " << name << " has "
          << "nrows=" << nrows << ", while its rowindex.length="
          << col_ri->length << end;
    }

    // Check that the maximum value of the RowIndex does not exceed the maximum
    // row number in the memory buffer
    if (col_ri->max >= mbuf_nrows && col_ri->max > 0) {
      icc << "Maximum row number in the rowindex of " << name << " exceeds the "
          << "number of rows in the underlying memory buffer: max(rowindex)="
          << col_ri->max << ", and nrows(membuf)=" << mbuf_nrows << end;
    }
  }
  else {
    // Check that nrows is a correct representation of mbuf's size
    if (nrows != mbuf_nrows) {
      icc << "Mismatch between reported number of rows: " << name
          << " has nrows=" << nrows << " and MemoryBuffer has data for "
          << mbuf_nrows << " rows" << end;
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
