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
Column* StringColumn<T>::extract_simple_slice(RowIndex* rowindex) const {
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
  ((void)rowindex);
  return NULL;
}


// Explicit instantiation of the template
template class StringColumn<int32_t>;
template class StringColumn<int64_t>;
