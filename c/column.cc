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
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>    // memcpy, strcmp
#include <cstdlib>     // atoll
#include <errno.h>
#include <fcntl.h>     // open
#include <unistd.h>    // close
#include "utils.h"
#include "myassert.h"
#include "rowindex.h"
#include "sort.h"
#include "py_utils.h"


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
      meta(nullptr),
      nrows(nrows_),
      stats(Stats::void_ptr()) {}


Column* Column::new_column(SType stype) {
  switch (stype) {
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
    case ST_VOID:            return new Column(0);  // FIXME
    default:
      THROW_ERROR("Unable to create a column of SType = %d\n", stype);
  }
}


Column* Column::new_data_column(SType stype, int64_t nrows) {
  Column* col = new_column(stype);
  col->nrows = nrows;
  col->mbuf = new MemoryMemBuf(allocsize0(stype, nrows));
  return col;
}


Column* Column::new_mmap_column(SType stype, int64_t nrows,
                                const char* filename) {
  size_t sz = allocsize0(stype, nrows);
  Column* col = new_column(stype);
  col->nrows = nrows;
  col->mbuf = new MemmapMemBuf(filename, sz, /* create = */ true);
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
  // Open and memory-map the file
  int fd = open(filename, O_RDWR|O_CREAT, 0666);
  if (fd == -1) {
    close(fd);
    throw new Error("Cannot open file %s", filename);
  }
  size_t sz = mbuf->size();
  int ret = ftruncate(fd, (off_t) sz);
  if (ret == -1) {
    close(fd);
    throw new Error("Cannot truncate file %s", filename);
  }
  void *mmp = mmap(NULL, sz, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);
  if (mmp == MAP_FAILED) throw new Error("Memory-map failed.");

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
  col->mbuf = new MemmapMemBuf(filename, 0, /* create = */ false);
  if (col->alloc_size() < allocsize0(stype, nrows)) {
    throw new Error("File %s has size %zu, which is not sufficient for a column"
                    " with %zd rows", filename, col->alloc_size(), nrows);
  }
  // Deserialize the meta information, if needed
  if (stype == ST_STRING_I4_VCHAR || stype == ST_STRING_I8_VCHAR) {
    if (strncmp(ms, "offoff=", 7) != 0)
      throw new Error("Cannot retrieve required metadata in string \"%s\"", ms);
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
Column* Column::shallowcopy(RowIndex* new_rowindex) {
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
  col->mbuf = new MemoryMemBuf(*mbuf);
  if (meta) {
    memcpy(col->meta, meta, stype_info[stype()].metasize);
  }
  // TODO: deep copy stats when implemented
  col->ri = rowindex() == nullptr ? nullptr : new RowIndex(rowindex());
  return col;
}


// FIXME: this method should be pure virtual
SType Column::stype() const {
  return ST_VOID;
}


// FIXME: this method should be pure virtual
int64_t Column::data_nrows() const {
  return 0;
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


/**
 * Extract data from this column at rows specified in the provided `rowindex`.
 * A new Column with type `MT_DATA` will be created and returned. The
 * `rowindex` parameter can also be NULL, in which case a shallow copy
 * is returned (if a "deep" copy is needed, then use `column_copy()`).
 */
Column* Column::extract() {
  // If `rowindex` is not provided, then return a shallow "copy".
  if (rowindex() == nullptr) {
    return shallowcopy();
  }

  size_t res_nrows = (size_t) nrows;
  size_t elemsize =
      (stype() == ST_STRING_FCHAR) ?
          (size_t) ((FixcharMeta*) meta)->n : stype_info[stype()].elemsize;

  // Create a new `Column` object.
  // TODO: Stats should be copied from DataTable
  Column *res = Column::new_data_column(stype(), 0);
  res->nrows = (int64_t) res_nrows;

  // "Slice" rowindex with step = 1 is a simple subsection of the column
  if (ri->type == RI_SLICE && ri->slice.step == 1) {
    size_t start = (size_t) ri->slice.start;
    switch (stype()) {
    case ST_STRING_I4_VCHAR: {
      size_t offoff = (size_t) ((VarcharMeta*) meta)->offoff;
      int32_t *offs = (int32_t*) mbuf->at(offoff) + start;
      int32_t off0 = start ? abs(*(offs - 1)) - 1 : 0;
      int32_t off1 = start + res_nrows ? abs(*(offs + res_nrows - 1)) - 1 : 0;
      size_t datasize = (size_t) (off1 - off0);
      size_t padding = i4s_padding(datasize);
      size_t offssize = res_nrows * elemsize;
      offoff = datasize + padding;
      res->mbuf->resize(datasize + padding + offssize);
      ((VarcharMeta*) res->meta)->offoff = (int64_t) offoff;
      memcpy(res->data(), mbuf->at(off0), datasize);
      memset(res->mbuf->at(datasize), 0xFF, padding);
      int32_t *resoffs = (int32_t*) res->mbuf->at(offoff);
      for (size_t i = 0; i < res_nrows; ++i) {
        resoffs[i] = offs[i] > 0 ? offs[i] - off0 : offs[i] + off0;
      }
      break;
    }

    case ST_STRING_I8_VCHAR: {
      size_t offoff = (size_t) ((VarcharMeta*) meta)->offoff;
      int64_t *offs = (int64_t*) mbuf->at(offoff) + start;
      int64_t off0 = start ? llabs(*(offs - 1)) - 1 : 0;
      int64_t off1 = start + res_nrows ? llabs(*(offs + res_nrows - 1)) - 1 : 0;
      size_t datasize = (size_t) (off1 - off0);
      size_t padding = i8s_padding(datasize);
      size_t offssize = res_nrows * elemsize;
      offoff = datasize + padding;
      res->mbuf->resize(datasize + padding + offssize);
      ((VarcharMeta*) res->meta)->offoff = (int64_t) offoff;
      memcpy(res->data(), mbuf->at(off0), datasize);
      memset(res->mbuf->at(datasize), 0xFF, padding);
      int64_t *resoffs = (int64_t*) res->mbuf->at(offoff);
      for (size_t i = 0; i < res_nrows; ++i) {
        resoffs[i] = offs[i] > 0 ? offs[i] - off0 : offs[i] + off0;
      }
      break;
    }

    case ST_STRING_U1_ENUM:
    case ST_STRING_U2_ENUM:
    case ST_STRING_U4_ENUM:
      assert(0);  // not implemented yet
      break;

    default: {
      assert(!stype_info[stype()].varwidth);
      size_t res_alloc_size = res_nrows * elemsize;
      size_t offset = start * elemsize;
      res->mbuf->resize(res_alloc_size);
      memcpy(res->data(), mbuf->at(offset), res_alloc_size);
    }
    }
    return res;
  }

  // In all other cases we need to iterate through the rowindex and fetch
  // the required elements manually.
  switch (stype()) {
        #define JINIT_SLICE                                                    \
            int64_t start = ri->slice.start;                                   \
            int64_t step = ri->slice.step;                                     \
            int64_t j = start - step;
        #define JITER_SLICE                                                    \
            j += step;
        #define JINIT_ARR(bits)                                                \
            intXX(bits) *rowindices = ri->ind ## bits;
        #define JITER_ARR(bits)                                                \
            intXX(bits) j = rowindices[i];

        #define CASE_IX_VCHAR_SUB(ctype, abs, JINIT, JITER) {                  \
            size_t offoff = (size_t)((VarcharMeta*) meta)->offoff;             \
            ctype *offs = (ctype*) mbuf->at(offoff);                           \
            size_t datasize = 0;                                               \
            {   JINIT                                                          \
                for (size_t i = 0; i < res_nrows; ++i) {                       \
                    JITER                                                      \
                    if (offs[j] > 0) {                                         \
                        ctype prevoff = j? abs(offs[j - 1]) : 1;               \
                        datasize += (size_t)(offs[j] - prevoff);               \
                    }                                                          \
                }                                                              \
            }                                                                  \
            size_t padding = elemsize == 4 ? i4s_padding(datasize)             \
                                           : i8s_padding(datasize);            \
            size_t offssize = res_nrows * elemsize;                            \
            offoff = datasize + padding;                                       \
            res->mbuf->resize(offoff + offssize);                              \
            ((VarcharMeta*) res->meta)->offoff = (int64_t) offoff;             \
            {   JINIT                                                          \
                ctype lastoff = 1;                                             \
                char *dest = static_cast<char*>(res->data());                  \
                ctype *resoffs = (ctype*) res->mbuf->at(offoff);               \
                for (size_t i = 0; i < res_nrows; ++i) {                       \
                    JITER                                                      \
                    if (offs[j] > 0) {                                         \
                        ctype prevoff = j? abs(offs[j - 1]) : 1;               \
                        size_t len = (size_t)(offs[j] - prevoff);              \
                        if (len) {                                             \
                            memcpy(dest, mbuf->at((size_t)(prevoff - 1)), len);\
                            dest += len;                                       \
                            lastoff += len;                                    \
                        }                                                      \
                        resoffs[i] = lastoff;                                  \
                    } else                                                     \
                        resoffs[i] = -lastoff;                                 \
                }                                                              \
                memset(dest, 0xFF, padding);                                   \
            }                                                                  \
        }
        #define CASE_IX_VCHAR(ctype, abs)                                      \
            if (ri->type == RI_SLICE)                                          \
                CASE_IX_VCHAR_SUB(ctype, abs, JINIT_SLICE, JITER_SLICE)        \
            else if (ri->type == RI_ARR32)                                     \
                CASE_IX_VCHAR_SUB(ctype, abs, JINIT_ARR(32), JITER_ARR(32))    \
            else if (ri->type == RI_ARR64)                                     \
                CASE_IX_VCHAR_SUB(ctype, abs, JINIT_ARR(64), JITER_ARR(64))    \
            break;

  case ST_STRING_I4_VCHAR:
    CASE_IX_VCHAR(int32_t, abs)
  case ST_STRING_I8_VCHAR:
    CASE_IX_VCHAR(int64_t, llabs)

  #undef CASE_IX_VCHAR_SUB
  #undef CASE_IX_VCHAR
  #undef JINIT_SLICE
  #undef JINIT_ARRAY
  #undef JITER_SLICE
  #undef JITER_ARRAY

  case ST_STRING_U1_ENUM:
  case ST_STRING_U2_ENUM:
  case ST_STRING_U4_ENUM:
    assert(0);  // not implemented yet
    break;

  default: {
    assert(!stype_info[stype()].varwidth);
    res->mbuf->resize(res_nrows * elemsize);
    char *dest = (char*) res->data();
    if (ri->type == RI_SLICE) {
      size_t startsize = (size_t) ri->slice.start * elemsize;
      size_t stepsize = (size_t) ri->slice.step * elemsize;
      char *src = (char*) (mbuf->get()) + startsize;
      for (size_t i = 0; i < res_nrows; ++i) {
        memcpy(dest, src, elemsize);
        dest += elemsize;
        src += stepsize;
      }
    } else if (ri->type == RI_ARR32) {
      int32_t *rowindices = ri->ind32;
      for (size_t i = 0; i < res_nrows; ++i) {
        size_t j = (size_t) rowindices[i];
        memcpy(dest, mbuf->at(j * elemsize), elemsize);
        dest += elemsize;
      }
    } else if (ri->type == RI_ARR32) {
      int64_t *rowindices = ri->ind64;
      for (size_t i = 0; i < res_nrows; ++i) {
        size_t j = (size_t) rowindices[i];
        memcpy(dest, mbuf->at(j * elemsize), elemsize);
        dest += elemsize;
      }
    }
    break;
  }
  }
  return res;
}


// FIXME: this method should be declared pure virtual
void Column::resize_and_fill(int64_t) {}


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
    res->stats->reset();

    // Use the appropriate strategy to continue appending the columns.
    res->rbind_impl(columns, new_nrows, col_empty);

    // If everything is fine, then the current column can be safely discarded
    // -- the upstream caller will replace this column with the `res`.
    if (res != this) delete this;
    return res;
}


// FIXME: this method should be declared pure virtual
void Column::rbind_impl(const std::vector<const Column*>&, int64_t, bool) {}


Column::~Column() {
  dtfree(meta);
  Stats::destruct(stats);
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

