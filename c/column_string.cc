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
#include "datatable_check.h"
#include "encodings.h"
#include <limits> // numeric_limits::max()


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
void StringColumn<T>::replace_buffer(MemoryBuffer* new_offbuf,
                                     MemoryBuffer* new_strbuf)
{
  int64_t new_nrows = new_offbuf->size()/sizeof(T) - 1;
  if (new_offbuf->size() % sizeof(T)) {
    throw Error("The size of `new_offbuf` is not a multiple of "
                STRINGIFY(sizeof(T)));
  }
  if (new_offbuf->get_elem<T>(0) != -1) {
    throw Error("Cannot use `new_offbuf` as an \"offsets\" buffer: first "
                "element of this array is not -1");
  }
  // MemoryBuffer* t = new_offbuf->shallowcopy();
  // if (mbuf) mbuf->release();
  // mbuf = t;
  // t = new_strbuf->shallowcopy();
  // if (strbuf) strbuf->release();
  // strbuf = t;

  nrows = new_nrows;
  //---- Temporary -----
  size_t strdata_size = new_strbuf->size();
  size_t padding_size = padding(strdata_size);
  size_t offsets_size = sizeof(T) * static_cast<size_t>(nrows);
  size_t final_size = strdata_size + padding_size + offsets_size;
  size_t offoff = strdata_size + padding_size;
  new_strbuf->resize(final_size);
  memset(new_strbuf->at(strdata_size), 0xFF, padding_size);
  memcpy(new_strbuf->at(offoff), new_offbuf->at(sizeof(T)), offsets_size);
  if (mbuf) mbuf->release();
  mbuf = new_strbuf->shallowcopy();
  ((VarcharMeta*) meta)->offoff = static_cast<int64_t>(offoff);
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
size_t StringColumn<T>::datasize() const{
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
  if (strbuf) return static_cast<char*>(strbuf->get()) - 1;
  return static_cast<char*>(mbuf->get()) - 1;
}

template <typename T>
T* StringColumn<T>::offsets() const {
  if (strbuf) return static_cast<T*>(mbuf->get());
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
  if (stats != nullptr) stats->reset();
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
  if (stats != nullptr) stats->reset();
}

template <typename T>
void StringColumn<T>::fill_na() {
  int64_t mbuf_nrows = data_nrows();
  mbuf->resize(static_cast<size_t>(mbuf_nrows) * sizeof(T) + padding(0));
  ((VarcharMeta*) meta)->offoff = (int64_t) padding(0);
  memset(mbuf->get(), 0xFF, static_cast<size_t>(mbuf_nrows) * sizeof(T) + padding(0));
}

//---- Stats -------------------------------------------------------------------

template <typename T>
StringStats<T>* StringColumn<T>::get_stats() {
  if (stats == nullptr) stats = new StringStats<T>();
  return static_cast<StringStats<T>*>(stats);
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


//------------------------------------------------------------------------------
// Integrity checks
//------------------------------------------------------------------------------

template <typename T>
bool StringColumn<T>::verify_integrity(
    IntegrityCheckContext& icc, const std::string& name) const
{
  bool r = Column::verify_integrity(icc, name);
  if (!r) return false;
  int nerrors = icc.n_errors();
  auto end = icc.end();

  // Check general properties
  // Note: meta value is implicitly checked here
  int64_t strdata_size = 0;
  int64_t offoff = ((VarcharMeta*) meta)->offoff;
  //*_utf8 functions use unsigned char*
  const unsigned char *cdata = (unsigned char*) strdata();
  const T *str_offsets = offsets();

  // Check that the offsets section is preceded by a -1
  if (str_offsets[-1] != -1) {
    icc << "Offsets section in (string) " << name << " is not preceded by "
        << "number -1" << end;
  }
  int64_t mbuf_nrows = data_nrows();
  T lastoff = 1;

  // Check for the validity of each offset
  for (int64_t i = 0; i < mbuf_nrows; ++i) {
    T oj = str_offsets[i];
    if (oj < 0 && oj != -lastoff) {
      icc << "Offset of NA String in row " << i << " of " << name << " does not"
          << " have the same magnitude as the previous offset: offset = " << oj
          << ", previous offset = " << lastoff << end;
    } else if (oj >= 0 && oj < lastoff) {
      icc << "String offset in row " << i << " of " << name << " cannot be less"
          << " than the previous offset: offset = " << oj << ", previous offset"
          << " = " << lastoff << end;
    }
    if (oj - 1 > offoff) {
      icc << "String offset in row " << i << " of " << name << " is greater "
          << "than the length of the string data region: offset = " << oj
          << ", region length = " << offoff << end;
    }
    else if (oj > 0 && !is_valid_utf8(cdata + lastoff,
                                      static_cast<size_t>(oj - lastoff))) {
      icc << "Invalid UTF-8 string in row " << i << " of " << name << ": "
          << repr_utf8(cdata + lastoff, cdata + oj) << end;
    }
    lastoff = std::abs(oj);
  }
  strdata_size = static_cast<int64_t>(lastoff) - 1;

  // Check that the space between the string data and offset section is
  // filled with 0xFFs
  for (int64_t i = strdata_size; i < offoff; ++i) {
    if (cdata[i+1] != 0xFF) {
      icc << "String data section in " << name << " is not padded with '0xFF's"
          << " at offset " << i << end;
      break; // Do not report this error more than once
    }
  }

  return !icc.has_errors(nerrors);
}


// TODO: once `meta` is properly removed, make sure that all actually useful
//       checks are transferred to the ::verify_integrity() function.
/*
template <typename T>
int StringColumn<T>::verify_meta_integrity(
    std::vector<char> *errors, int max_errors, const char *name) const
{
  int nerrors = Column::verify_meta_integrity(errors, max_errors, name);
  if (nerrors > 0) {
    return nerrors;
  }
  if (errors == nullptr) return nerrors; // TODO: do something else?

  // Check that the meta is not null
  if (meta == nullptr) {
    ERR("Meta information in %s of String type is null\n", name);
    return nerrors;
  } else {
    // Check for a valid meta size
    size_t meta_alloc = array_size(meta, 1);
    if (meta_alloc > 0 && meta_alloc < 8) {
      ERR("Inconsistent meta info structure in %s of String type: %d bytes "
          "expected, but only %llu bytes were allocated\n",
          name, 8, meta_alloc);
      return nerrors;
    }
  }

  int64_t offoff = ((VarcharMeta*) meta)->offoff;

  // Check that the meta value is positive
  if (offoff <= 0) {
    ERR("`meta` data in %s of type String reports a nonpositive data length: "
        "%lld\n", name, offoff);
    return nerrors;
  }

  // Check that the meta value is at least the size of one integer
  // (since offset data must be precluded with a -1)
  if ((size_t) offoff < sizeof(T)) {
    ERR("`meta` data in %s of type String reports an data length smaller "
        "than the element size of %llu: %lld\n", name, sizeof(T), offoff);
    return nerrors;
  }

  // Check that the meta value is a multiple of 8
  if ((offoff & 7) != 0) {
    ERR("`meta` data in %s of type String reports an data length that is not "
        "a multiple of 8: %lld\n", name, offoff);
    return nerrors;
  }

  // Check that the meta value is not longer than the maximum int value
  if (offoff > std::numeric_limits<T>::max()) {
    ERR("`meta` data in %s of type String reports a data length that "
        "cannot be represented in %llu bytes: %lld\n", name, sizeof(T), offoff);
    return nerrors;
  }

  // Calculate the meta value through other means and compare it with the
  // reported value
  size_t exp_padding = padding(datasize());
  if ((size_t) offoff != datasize() + exp_padding) {
    ERR("Mismatch between data length reported by `meta` and calculated "
        "data length in %s of type String: `meta` reported %lld, but "
        "calculated %llu\n", name, offoff, datasize() + exp_padding);
    return nerrors;
  }

  // Check that the padding is all 0xFFs
  uint8_t *cdata = (uint8_t*) data();
  for (size_t i = 0; i < exp_padding; ++i) {
    uint8_t c = *(cdata + datasize() + i);
    if (c != 0xFF) {
      ERR("String data of %s is not correctly padded with 0xFF bytes: "
          "byte %X is %X\n", name, datasize() + i, c);
      return nerrors;
    }
  }

  return nerrors;
}
*/


// Explicit instantiation of the template
template class StringColumn<int32_t>;
template class StringColumn<int64_t>;
