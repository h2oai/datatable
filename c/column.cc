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


Column::Column(int64_t nrows_)
    : mbuf(nullptr),
      meta(nullptr),
      nrows(nrows_),
      stats(Stats::void_ptr()),
      refcount(1) {}


size_t Column::allocsize0(SType stype, size_t n) {
  size_t sz = n * stype_info[stype].elemsize;
  if (stype == ST_STRING_I4_VCHAR) sz += i4s_padding(0);
  if (stype == ST_STRING_I8_VCHAR) sz += i8s_padding(0);
  return sz;
}


Column::Column(size_t nrows_, SType stype_)
    : mbuf(nullptr),
      meta(nullptr),
      nrows(static_cast<int64_t>(nrows_)),
      stats(Stats::void_ptr()),
      refcount(1),
      _stype(stype_)
{
    size_t meta_size = stype_info[stype_].metasize;
    if (meta_size) {
        meta = malloc(meta_size);
        if (stype_ == ST_STRING_I4_VCHAR)
            ((VarcharMeta*) meta)->offoff = (int64_t) i4s_padding(0);
        if (stype_ == ST_STRING_I8_VCHAR)
            ((VarcharMeta*) meta)->offoff = (int64_t) i8s_padding(0);
    }
}


/**
 * Simple Column constructor: create a Column with memory type `MT_DATA` and
 * having the provided SType. The column will be preallocated for `nrows` rows,
 * and will have `refcount` = 1. For a variable-width column only the "fixed-
 * -width" part will be allocated.
 *
 * If the column cannot be created (probably due to Out-of-Memory exception),
 * the function will return NULL.
 */
Column::Column(SType stype_, size_t nrows_)
    : Column(nrows_, stype_)
{
  mbuf = new MemoryMemBuf(allocsize0(stype_, nrows_));
}


Column::Column(SType stype_, size_t nrows_, const char* filename)
    : Column(nrows_, stype_)
{
  size_t sz = allocsize0(stype_, nrows_);
  mbuf = new MemmapMemBuf(filename, sz, /* create = */ true);
}



/**
 * Save this column's data buffer into file `filename`. Other information
 * about the column should be stored elsewhere (for example in the _meta.nff
 * file).
 * If a file with the given name already exists, it will be overwritten.
 */
Column* Column::save_to_disk(const char *filename)
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
Column::Column(const char* filename, SType st, size_t nr, const char* ms)
    : Column(nr, st)
{
  mbuf = new MemmapMemBuf(filename, 0, /* create = */ false);
  // Deserialize the meta information, if needed
  if (st == ST_STRING_I4_VCHAR || st == ST_STRING_I8_VCHAR) {
    if (strncmp(ms, "offoff=", 7) != 0)
      throw new Error("Cannot retrieve required metadata in string \"%s\"", ms);
    int64_t offoff = (int64_t) atoll(ms + 7);
    ((VarcharMeta*) meta)->offoff = offoff;
  }
}


/**
 * Construct a column from the externally provided buffer.
 */
Column::Column(SType st, size_t nr, void* pybuffer, void* data, size_t a_size)
    : Column(nr, st)
{
  mbuf = new ExternalMemBuf(data, pybuffer, a_size);
}



/**
 * Make a "deep" copy of the column. The column created with this method will
 * have memory-type MT_DATA and refcount of 1.
 * If a shallow copy is needed, then simply copy the column's reference and
 * call `column_incref()`.
 */
Column::Column(const Column &other)
    : Column(static_cast<size_t>(other.nrows), other.stype())
{
  mbuf = new MemoryMemBuf(other.alloc_size());
  // TODO: deep copy stats when implemented
  if (alloc_size()) {
    memcpy(data(), other.data(), alloc_size());
  }
  size_t meta_size = stype_info[stype()].metasize;
  if (meta_size) {
    memcpy(meta, other.meta, meta_size);
  }
}


SType Column::stype() const {
  return _stype;
}

void* Column::data() const {
  return mbuf->get();
}

void* Column::data_at(size_t i) const {
  return mbuf->at(i);
}

size_t Column::alloc_size() const {
  return mbuf->size();
}

PyObject* Column::mbuf_repr() const {
  return mbuf->pyrepr();
}


/**
 * Extract data from this column at rows specified in the provided `rowindex`.
 * A new Column with type `MT_DATA` will be created and returned. The
 * `rowindex` parameter can also be NULL, in which case a shallow copy
 * is returned (if a "deep" copy is needed, then use `column_copy()`).
 */
Column* Column::extract(RowIndex *rowindex) {
    // If `rowindex` is not provided, then return a shallow "copy".
    if (rowindex == NULL) {
        return incref();
    }

    size_t res_nrows = (size_t) rowindex->length;
    size_t elemsize = (stype() == ST_STRING_FCHAR)
                        ? (size_t) ((FixcharMeta*) meta)->n
                        : stype_info[stype()].elemsize;

    // Create the new Column object.
    // TODO: Stats should be copied from DataTable
    Column *res = new Column(stype(), 0);
    res->nrows = (int64_t) res_nrows;

    // "Slice" rowindex with step = 1 is a simple subsection of the column
    if (rowindex->type == RI_SLICE && rowindex->slice.step == 1) {
        size_t start = (size_t) rowindex->slice.start;
        switch (stype()) {
            case ST_STRING_I4_VCHAR: {
                size_t offoff = (size_t)((VarcharMeta*) meta)->offoff;
                int32_t *offs = (int32_t*) mbuf->at(offoff) + start;
                int32_t off0 = start ? abs(*(offs - 1)) - 1 : 0;
                int32_t off1 = start + res_nrows ?
                               abs(*(offs + res_nrows - 1)) - 1 : 0;
                size_t datasize = (size_t)(off1 - off0);
                size_t padding = i4s_padding(datasize);
                size_t offssize = res_nrows * elemsize;
                offoff = datasize + padding;
                res->mbuf->resize(datasize + padding + offssize);
                ((VarcharMeta*) res->meta)->offoff = (int64_t)offoff;
                memcpy(res->data(), mbuf->at(off0), datasize);
                memset(res->mbuf->at(datasize), 0xFF, padding);
                int32_t *resoffs = (int32_t*) res->mbuf->at(offoff);
                for (size_t i = 0; i < res_nrows; ++i) {
                    resoffs[i] = offs[i] > 0? offs[i] - off0
                                            : offs[i] + off0;
                }
            } break;

            case ST_STRING_I8_VCHAR: {
                size_t offoff = (size_t)((VarcharMeta*) meta)->offoff;
                int64_t *offs = (int64_t*) mbuf->at(offoff) + start;
                int64_t off0 = start ? llabs(*(offs - 1)) - 1 : 0;
                int64_t off1 = start + res_nrows ?
                               llabs(*(offs + res_nrows - 1)) - 1 : 0;
                size_t datasize = (size_t)(off1 - off0);
                size_t padding = i8s_padding(datasize);
                size_t offssize = res_nrows * elemsize;
                offoff = datasize + padding;
                res->mbuf->resize(datasize + padding + offssize);
                ((VarcharMeta*) res->meta)->offoff = (int64_t)offoff;
                memcpy(res->data(), mbuf->at(off0), datasize);
                memset(res->mbuf->at(datasize), 0xFF, padding);
                int64_t *resoffs = (int64_t*) res->mbuf->at(offoff);
                for (size_t i = 0; i < res_nrows; ++i) {
                    resoffs[i] = offs[i] > 0? offs[i] - off0
                                            : offs[i] + off0;
                }
            } break;

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
            } break;
        }
        return res;
    }

    // In all other cases we need to iterate through the rowindex and fetch
    // the required elements manually.
    switch (stype()) {
        #define JINIT_SLICE                                                    \
            int64_t start = rowindex->slice.start;                             \
            int64_t step = rowindex->slice.step;                               \
            int64_t j = start - step;
        #define JITER_SLICE                                                    \
            j += step;
        #define JINIT_ARR(bits)                                                \
            intXX(bits) *rowindices = rowindex->ind ## bits;
        #define JITER_ARR(bits)                                                \
            intXX(bits) j = rowindices[i];

        #define CASE_IX_VCHAR_SUB(ctype, abs, JINIT, JITER) {                  \
            size_t offoff = (size_t)((VarcharMeta*) meta)->offoff;             \
            ctype *offs = (ctype*) mbuf->at(offoff);                            \
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
            res->mbuf->resize(offoff + offssize);                               \
            ((VarcharMeta*) res->meta)->offoff = (int64_t) offoff;             \
            {   JINIT                                                          \
                ctype lastoff = 1;                                             \
                char *dest = static_cast<char*>(res->data());                  \
                ctype *resoffs = (ctype*) res->mbuf->at(offoff);                \
                for (size_t i = 0; i < res_nrows; ++i) {                       \
                    JITER                                                      \
                    if (offs[j] > 0) {                                         \
                        ctype prevoff = j? abs(offs[j - 1]) : 1;               \
                        size_t len = (size_t)(offs[j] - prevoff);              \
                        if (len) {                                             \
                            memcpy(dest, mbuf->at((size_t)(prevoff - 1)), len); \
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
            if (rowindex->type == RI_SLICE)                                    \
                CASE_IX_VCHAR_SUB(ctype, abs, JINIT_SLICE, JITER_SLICE)        \
            else if (rowindex->type == RI_ARR32)                               \
                CASE_IX_VCHAR_SUB(ctype, abs, JINIT_ARR(32), JITER_ARR(32))    \
            else if (rowindex->type == RI_ARR64)                               \
                CASE_IX_VCHAR_SUB(ctype, abs, JINIT_ARR(64), JITER_ARR(64))    \
            break;

        case ST_STRING_I4_VCHAR: CASE_IX_VCHAR(int32_t, abs)
        case ST_STRING_I8_VCHAR: CASE_IX_VCHAR(int64_t, llabs)

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
            if (rowindex->type == RI_SLICE) {
                size_t startsize = (size_t) rowindex->slice.start * elemsize;
                size_t stepsize = (size_t) rowindex->slice.step * elemsize;
                char *src = (char*)(mbuf->get()) + startsize;
                for (size_t i = 0; i < res_nrows; ++i) {
                    memcpy(dest, src, elemsize);
                    dest += elemsize;
                    src += stepsize;
                }
            } else
            if (rowindex->type == RI_ARR32) {
                int32_t *rowindices = rowindex->ind32;
                for (size_t i = 0; i < res_nrows; ++i) {
                    size_t j = (size_t) rowindices[i];
                    memcpy(dest, mbuf->at(j*elemsize), elemsize);
                    dest += elemsize;
                }
            } else
            if (rowindex->type == RI_ARR32) {
                int64_t *rowindices = rowindex->ind64;
                for (size_t i = 0; i < res_nrows; ++i) {
                    size_t j = (size_t) rowindices[i];
                    memcpy(dest, mbuf->at(j*elemsize), elemsize);
                    dest += elemsize;
                }
            }
        } break;
    }
    return res;
}

/**
 * Expand the column up to `nrows` elements and return it. The pointer returned
 * will be `self` if the column can be modified in-place, or a new Column object
 * otherwise. In the latter case the original object `self` will be decrefed.
 *
 * The column itself will be expanded in the following way: if it had just a
 * single row, then this value will be repeated `nrows` times. If the column had
 * `nrows != 1`, then all extra rows will be filled with NAs.
 */
void Column::resize_and_fill(int64_t new_nrows)
{
    size_t old_nrows = (size_t) nrows;
    size_t diff_rows = (size_t) new_nrows - old_nrows;
    size_t old_alloc_size = alloc_size();
    assert(diff_rows > 0);

    if (!stype_info[stype()].varwidth) {
        size_t elemsize = stype_info[stype()].elemsize;
        size_t newsize = elemsize * static_cast<size_t>(new_nrows);
        if (mbuf->is_readonly()) {
          // The buffer is readonly: make a copy of that buffer.
          MemoryBuffer *new_mbuf = new MemoryMemBuf(newsize);
          memcpy(new_mbuf->get(), mbuf->get(), old_alloc_size);
          mbuf->release();
          mbuf = new_mbuf;
        } else {
          // The buffer is not readonly: expand it in-place
          mbuf->resize(newsize);
        }
        nrows = new_nrows;

        // Replicate the value or fill with NAs
        size_t fill_size = elemsize * diff_rows;
        assert(alloc_size() - old_alloc_size == fill_size);
        if (old_nrows == 1) {
            set_value(mbuf->at(old_alloc_size), data(),
                      elemsize, diff_rows);
        } else {
            const void *na = stype_info[stype()].na;
            set_value(mbuf->at(old_alloc_size), na,
                      elemsize, diff_rows);
        }
        // TODO: Temporary fix. To be resolved in #301
        this->stats->reset();
    }
    else if (stype() == ST_STRING_I4_VCHAR) {
        if (new_nrows > INT32_MAX)
          THROW_ERROR("Nrows is too big for an i4s column: %lld", new_nrows);

        size_t old_data_size = i4s_datasize();
        size_t old_offoff = (size_t) ((VarcharMeta*) meta)->offoff;
        size_t new_data_size = old_data_size;
        if (old_nrows == 1) new_data_size = old_data_size * (size_t) new_nrows;
        size_t new_padding_size = Column::i4s_padding(new_data_size);
        size_t new_offoff = new_data_size + new_padding_size;
        size_t new_alloc_size = new_offoff + 4 * (size_t) new_nrows;
        assert(new_alloc_size > old_alloc_size);

        // DATA column with refcount 1: expand in-place
        if (mbuf->is_readonly()) {
          MemoryBuffer* new_mbuf = new MemoryMemBuf(new_alloc_size);
          memcpy(new_mbuf->get(), mbuf->get(), old_data_size);
          memcpy(new_mbuf->at(new_offoff), mbuf->at(old_offoff), 4 * old_nrows);
          mbuf->release();
          mbuf = new_mbuf;
        }
        // Otherwise create a new column and copy over the data
        else {
          mbuf->resize(new_alloc_size);
          if (old_offoff != new_offoff) {
            memmove(mbuf->at(new_offoff), mbuf->at(old_offoff), 4 * old_nrows);
          }
        }
        set_value(mbuf->at(new_data_size), NULL, 1, new_padding_size);
        ((VarcharMeta*) meta)->offoff = (int64_t) new_offoff;
        nrows = new_nrows;

        // Replicate the value, or fill with NAs
        int32_t *offsets = (int32_t*) mbuf->at(new_offoff);
        if (old_nrows == 1 && offsets[0] > 0) {
            set_value(mbuf->at(old_data_size), data(),
                      old_data_size, diff_rows);
            for (int32_t j = 0; j < (int32_t) new_nrows; ++j) {
                offsets[j] = 1 + (j + 1) * (int32_t) old_data_size;
            }
        } else {
            if (old_nrows == 1) assert(old_data_size == 0);
            assert(old_offoff == new_offoff && old_data_size == new_data_size);
            int32_t na = -(int32_t) new_data_size - 1;
            set_value(mbuf->at(old_alloc_size), &na, 4, diff_rows);
        }
        // TODO: Temporary fix. To be resolved in #301
        stats->reset();
    } else {
      throw new Error("Cannot realloc column of stype %d", stype());
    }
}




/**
 * Increase reference count on column `self`. This function should be called
 * when a new long-term copy to the column is created (for example if the column
 * is referenced from several data tables). For convenience, this function
 * returns the column object passed.
 * Here `self` can also be NULL, in which case this function does nothing.
 */
Column* Column::incref() {
    ++refcount;
    return this;
}

void Column::decref() {
}

Column::~Column() {
  // dtfree(meta);
  // Stats::destruct(stats);
  // mbuf->release();
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
 * Return the size of the data part in a ST_STRING_I(4|8)_VCHAR column.
 */
size_t Column::i4s_datasize() {
    assert(stype() == ST_STRING_I4_VCHAR);
    void *end = mbuf->at(alloc_size());
    return (size_t) abs(((int32_t*) end)[-1]) - 1;
}
size_t Column::i8s_datasize() {
    assert(stype() == ST_STRING_I8_VCHAR);
    void *end = mbuf->at(alloc_size());
    return (size_t) llabs(((int64_t*) end)[-1]) - 1;
}

/**
 * Get the total size of the memory occupied by this Column. This is different
 * from `column->alloc_size`, which in general reports byte size of the `data`
 * portion of the column.
 */
size_t Column::get_allocsize()
{
    size_t sz = sizeof(Column);
    sz += mbuf->memory_footprint();
    sz += stype_info[stype()].metasize;
    return sz;
}

