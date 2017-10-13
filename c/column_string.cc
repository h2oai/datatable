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
#include <cmath>  // abs
#include "py_utils.h"
#include "utils.h"


template <typename T> StringColumn<T>::StringColumn() : StringColumn<T>(0) {}


template <typename T>
StringColumn<T>::StringColumn(int64_t nrows) : Column(nrows)
{
  size_t sz = sizeof(T) * static_cast<size_t>(nrows);
  size_t pd = padding(0);
  mbuf = new MemoryMemBuf(sz + pd);
  // strbuf = new MemoryMemBuf(sz);

  // TODO: remove this
  strbuf = nullptr;
  meta = malloc(sizeof(VarcharMeta));
  ((VarcharMeta*) meta)->offoff = static_cast<int64_t>(pd);
}


template <typename T>
SType StringColumn<T>::stype() const {
  return stype_string(sizeof(T));
}


template <typename T>
StringColumn<T>::~StringColumn() {
  if (strbuf) strbuf->release();
}


template <typename T>
size_t StringColumn<T>::datasize() {
  size_t sz = mbuf->size();
  T* end = static_cast<T*>(mbuf->at(sz));
  return static_cast<size_t>(abs(end[-1]) - 1);
}

template <typename T>
size_t StringColumn<T>::padding(size_t datasize) {
  return ((8 - ((datasize + sizeof(T)) & 7)) & 7) + sizeof(T);
}

template <typename T>
int64_t StringColumn<T>::data_nrows() const {
  size_t offoff = static_cast<size_t>(((VarcharMeta*) meta)->offoff);
  return static_cast<int64_t>((mbuf->size() - offoff) / sizeof(T));
}

template <typename T>
char* StringColumn<T>::strdata() const {
  return static_cast<char*>(mbuf->get()) - 1;
}

template <typename T>
T* StringColumn<T>::offsets() const {
  int64_t offoff = ((VarcharMeta*) meta)->offoff;
  return static_cast<T*>(mbuf->at(offoff));
}



template <typename T>
Column* StringColumn<T>::extract_simple_slice(RowIndex*) const {
/*
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
*/
  return NULL;
}


template <typename T>
void StringColumn<T>::resize_and_fill(int64_t new_nrows)
{
  // TODO: clean this up
  size_t old_alloc_size = alloc_size();
  int64_t old_nrows = nrows;
  int64_t diff_rows = new_nrows - old_nrows;
  if (diff_rows == 0) return;
  if (diff_rows < 0) {
    throw Error("Column::resize_and_fill() cannot shrink a column");
  }

  if (new_nrows > INT32_MAX && sizeof(T) == 4) {
    // TODO: instead of throwing an error, upcast the column to <int64_t>
    THROW_ERROR("Nrows is too big for an i4s column: %lld", new_nrows);
  }

  size_t old_data_size = datasize();
  size_t old_offs_size = sizeof(T) * static_cast<size_t>(old_nrows);
  size_t old_offoff = (size_t) ((VarcharMeta*) meta)->offoff;
  size_t new_data_size = old_data_size;
  if (old_nrows == 1) new_data_size = old_data_size * (size_t) new_nrows;
  size_t new_offs_size = sizeof(T) * static_cast<size_t>(new_nrows);
  size_t new_padding_size = padding(new_data_size);
  size_t new_offoff = new_data_size + new_padding_size;
  size_t new_alloc_size = new_offoff + new_offs_size;
  assert(new_alloc_size > old_alloc_size);

  // DATA column with refcount 1: expand in-place
  if (mbuf->is_readonly()) {
    MemoryBuffer* new_mbuf = new MemoryMemBuf(new_alloc_size);
    memcpy(new_mbuf->get(), mbuf->get(), old_data_size);
    memcpy(new_mbuf->at(new_offoff), mbuf->at(old_offoff), old_offs_size);
    mbuf->release();
    mbuf = new_mbuf;
  }
  // Otherwise create a new column and copy over the data
  else {
    mbuf->resize(new_alloc_size);
    if (old_offoff != new_offoff) {
      memmove(mbuf->at(new_offoff), mbuf->at(old_offoff), old_offs_size);
    }
  }
  set_value(mbuf->at(new_data_size), NULL, 1, new_padding_size);
  ((VarcharMeta*) meta)->offoff = static_cast<int64_t>(new_offoff);
  nrows = new_nrows;

  // Replicate the value, or fill with NAs
  T *offsets = static_cast<T*>(mbuf->at(new_offoff));
  if (old_nrows == 1 && offsets[0] > 0) {
    set_value(mbuf->at(old_data_size), data(),
              old_data_size, static_cast<size_t>(diff_rows));
    for (T j = 0; j < static_cast<T>(new_nrows); ++j) {
      offsets[j] = 1 + (j + 1) * static_cast<T>(old_data_size);
    }
  } else {
    if (old_nrows == 1) assert(old_data_size == 0);
    assert(old_offoff == new_offoff && old_data_size == new_data_size);
    T na = -static_cast<T>(new_data_size + 1);
    set_value(mbuf->at(old_alloc_size), &na, sizeof(T),
              static_cast<size_t>(diff_rows));
  }
  // TODO: Temporary fix. To be resolved in #301
  stats->reset();
}


template <typename T>
void StringColumn<T>::rbind_impl(const std::vector<const Column*>& columns,
                                 int64_t new_nrows, bool col_empty)
{
  // Determine the size of the memory to allocate
  size_t old_nrows = (size_t) nrows;
  size_t old_offoff = 0;
  size_t new_data_size = 0;     // size of the string data region
  if (!col_empty) {
    old_offoff = (size_t) ((VarcharMeta*) meta)->offoff;
    new_data_size += datasize();
  }
  for (const Column* col : columns) {
    if (col->stype() == ST_VOID) continue;
    // TODO: replace with datasize(). But: what if col is not a string?
    int64_t offoff = ((VarcharMeta*) col->meta)->offoff;
    T *offsets = (T*) col->data_at(static_cast<size_t>(offoff));
    new_data_size += (size_t) abs(offsets[col->nrows - 1]) - 1;
  }
  size_t new_offsets_size = sizeof(T) * static_cast<size_t>(new_nrows);
  size_t padding_size = padding(new_data_size);
  size_t new_offoff = new_data_size + padding_size;
  size_t new_alloc_size = new_offoff + new_offsets_size;

  // Reallocate the column
  assert(new_alloc_size >= alloc_size());
  mbuf->resize(new_alloc_size);
  nrows = new_nrows;
  T *offsets = (T*) mbuf->at(new_offoff);
  ((VarcharMeta*) meta)->offoff = (int64_t) new_offoff;

  // Move the original offsets
  T rows_to_fill = 0;  // how many rows need to be filled with NAs
  T curr_offset = 0;   // Current offset within string data section
  if (col_empty) {
    rows_to_fill += old_nrows;
    offsets[-1] = -1;
  } else {
    memmove(offsets, mbuf->at(old_offoff), old_nrows * 4);
    offsets[-1] = -1;
    curr_offset = abs(offsets[old_nrows - 1]) - 1;
    offsets += old_nrows;
  }

  for (const Column* col : columns) {
    if (col->stype() == ST_VOID) {
      rows_to_fill += col->nrows;
    } else {
      if (rows_to_fill) {
        const T na = -curr_offset - 1;
        set_value(offsets, &na, sizeof(T), (size_t)rows_to_fill);
        offsets += rows_to_fill;
        rows_to_fill = 0;
      }
      size_t offoff = static_cast<size_t>(((VarcharMeta*) col->meta)->offoff);
      T *col_offsets = (T*) col->data_at(offoff);
      for (int64_t j = 0; j < col->nrows; j++) {
        T off = col_offsets[j];
        *offsets++ = off > 0? off + curr_offset : off - curr_offset;
      }
      size_t data_size = (size_t)(abs(col_offsets[col->nrows - 1]) - 1);
      memcpy(mbuf->at(curr_offset), col->data(), data_size);
      curr_offset += data_size;
    }
    delete col;
  }
  if (rows_to_fill) {
    const T na = -curr_offset - 1;
    set_value(offsets, &na, sizeof(T), (size_t)rows_to_fill);
  }
  if (padding_size) {
    memset(mbuf->at(new_offoff - padding_size), 0xFF, padding_size);
  }
}


template <typename T>
void StringColumn<T>::apply_na_mask(const BoolColumn* mask) {
  const int8_t* maskdata = mask->elements();
  char* strdata = this->strdata();
  T* offsets = this->offsets();

  // How much to reduce the offsets by due to some strings turning into NAs
  T doffset = 0;
  T offp = 1;
  for (int64_t j = 0; j < nrows; ++j) {
    T offi = offsets[j];
    T offa = abs(offi);
    if (maskdata[j] == 1) {
      doffset += offa - offp;
      offsets[j] = -offp;
      continue;
    } else if (doffset) {
      if (offi > 0) {
        offsets[j] = offi - doffset;
        memmove(strdata + offp, strdata + offp + doffset,
                static_cast<size_t>(offi - offp - doffset));
      } else {
        offsets[j] = -offp;
      }
    }
    offp = offa;
  }
}



//----- Type casts -------------------------------------------------------------

template <typename T>
void StringColumn<T>::cast_into(PyObjectColumn* target) const {
  char* strdata = this->strdata();
  T* offsets = this->offsets();
  PyObject** trg_data = target->elements();

  T prev_off = 1;
  for (int64_t i = 0; i < this->nrows; ++i) {
    T off = offsets[i];
    if (off < 0) {
      trg_data[i] = none();
    } else {
      T len = off - prev_off;
      trg_data[i] = PyUnicode_FromStringAndSize(strdata + prev_off, len);
      prev_off = off;
    }
  }
}


// Explicit instantiation of the template
template class StringColumn<int32_t>;
template class StringColumn<int64_t>;
