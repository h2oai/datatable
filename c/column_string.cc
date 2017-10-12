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
  size_t old_alloc_size = alloc_size();
  int64_t old_nrows = nrows;
  int64_t diff_rows = new_nrows - old_nrows;
  if (diff_rows == 0) return;
  if (diff_rows < 0) {
    throw Error("Column::resize_and_fill() cannot shrink a column");
  }

  if (new_nrows > INT32_MAX)
    THROW_ERROR("Nrows is too big for an i4s column: %lld", new_nrows);

  size_t old_data_size = datasize();
  size_t old_offoff = (size_t) ((VarcharMeta*) meta)->offoff;
  size_t new_data_size = old_data_size;
  if (old_nrows == 1) new_data_size = old_data_size * (size_t) new_nrows;
  size_t new_padding_size = padding(new_data_size);
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
              old_data_size, static_cast<size_t>(diff_rows));
    for (int32_t j = 0; j < (int32_t) new_nrows; ++j) {
      offsets[j] = 1 + (j + 1) * (int32_t) old_data_size;
    }
  } else {
    if (old_nrows == 1) assert(old_data_size == 0);
    assert(old_offoff == new_offoff && old_data_size == new_data_size);
    int32_t na = -(int32_t) new_data_size - 1;
    set_value(mbuf->at(old_alloc_size), &na, 4, static_cast<size_t>(diff_rows));
  }
  // TODO: Temporary fix. To be resolved in #301
  stats->reset();
}



// Explicit instantiation of the template
template class StringColumn<int32_t>;
template class StringColumn<int64_t>;
