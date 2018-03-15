//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "column.h"
#include <cmath>  // abs
#include <limits> // numeric_limits::max()
#include "py_utils.h"
#include "utils.h"
#include "datatable_check.h"
#include "encodings.h"
#include "utils/assert.h"

// Returns the expected path of the string data file given
// the path to the offsets
static std::string path_str(const std::string& path);

template <typename T>
StringColumn<T>::StringColumn() : Column(0) {
  strbuf = nullptr;
}


template <typename T>
StringColumn<T>::StringColumn(int64_t nrows_,
    MemoryBuffer* mb, MemoryBuffer* sb) : Column(nrows_)
{
  size_t exp_off_size = sizeof(T) * (static_cast<size_t>(nrows_) + 1);
  if (mb == nullptr) {
    if (sb != nullptr) {
      throw Error() << "String buffer cannot be defined when offset buffer is null";
    }
    mb = new MemoryMemBuf(exp_off_size);
    sb = new MemoryMemBuf(0);
    mb->set_elem<T>(0, -1);
  } else {
    if (sb == nullptr) {
      throw Error() << "String buffer cannot be null when offset buffer is defined";
    }
    assert(mb->size() == exp_off_size);
    assert(mb->get_elem<T>(0) == -1);
    size_t exp_str_size = static_cast<size_t>(abs(mb->get_elem<T>(nrows)) - 1);
    assert(sb->size() == exp_str_size);
  }

  mbuf = mb;
  strbuf = sb;
}


//==============================================================================
// Initialization methods
//==============================================================================

template <typename T>
void StringColumn<T>::init_data() {
  assert(!ri && !mbuf && !strbuf);
  strbuf = new MemoryMemBuf(0);
  mbuf = new MemoryMemBuf((static_cast<size_t>(nrows) + 1) * sizeof(T));
  mbuf->set_elem<T>(0, -1);
}

template <typename T>
void StringColumn<T>::init_mmap(const std::string& filename) {
  assert(!ri && !mbuf && !strbuf);
  strbuf = new MemmapMemBuf(path_str(filename), 0);
  mbuf = new MemmapMemBuf(filename, (static_cast<size_t>(nrows) + 1) * sizeof(T));
  mbuf->set_elem<T>(0, -1);
}

template <typename T>
void StringColumn<T>::open_mmap(const std::string& filename) {
  assert(!ri && !mbuf && !strbuf);

  mbuf = new MemmapMemBuf(filename);
  size_t exp_mbuf_size = sizeof(T) * (static_cast<size_t>(nrows) + 1);

  if (mbuf->size() != exp_mbuf_size) {
    size_t mbuf_size = mbuf->size();
    mbuf->release();
    throw Error() << "File \"" << filename <<
    "\" cannot be used to create a column with " << nrows <<
    " rows. Expected file size of " << exp_mbuf_size <<
    " bytes, actual size is " << mbuf_size << " bytes";
  }

  std::string filename_str = path_str(filename);

  strbuf = new MemmapMemBuf(filename_str);
  size_t exp_strbuf_size = static_cast<size_t>(abs(mbuf->get_elem<T>(nrows)) - 1);

  if (strbuf->size() != exp_strbuf_size) {
    size_t strbuf_size = strbuf->size();
    mbuf->release();
    strbuf->release();
    throw Error() << "File \"" << filename_str <<
      "\" cannot be used to create a column with " << nrows <<
      " rows. Expected file size of " << exp_strbuf_size <<
      " bytes, actual size is " << strbuf_size << " bytes";
  }
}

// Not implemented (should it be?) see method signature in `Column` for
// parameter definitions.
template <typename T>
void StringColumn<T>::init_xbuf(Py_buffer*) {
  throw Error() << "String columns are incompatible with external buffers";
}


//==============================================================================

template <typename T>
void StringColumn<T>::save_to_disk(const std::string& filename,
                                   WritableBuffer::Strategy strategy) {
  assert(mbuf != nullptr);
  assert(strbuf != nullptr);
  mbuf->save_to_disk(filename, strategy);
  strbuf->save_to_disk(path_str(filename), strategy);
}

template <typename T>
Column* StringColumn<T>::shallowcopy(const RowIndex& new_rowindex) const {
  Column* newcol = Column::shallowcopy(new_rowindex);
  StringColumn<T>* col = static_cast<StringColumn<T>*>(newcol);
  col->strbuf = strbuf->shallowcopy();
  return col;
}

template <typename T>
Column* StringColumn<T>::deepcopy() const {
  StringColumn<T>* col = static_cast<StringColumn<T>*>(Column::deepcopy());
  col->strbuf = strbuf->shallowcopy();
  return col;
}


template <typename T>
void StringColumn<T>::replace_buffer(MemoryBuffer* new_offbuf,
                                     MemoryBuffer* new_strbuf)
{
  assert(new_offbuf != nullptr);
  assert(new_strbuf != nullptr);
  int64_t new_nrows = new_offbuf->size()/sizeof(T) - 1;
  if (new_offbuf->size() % sizeof(T)) {
    throw ValueError() << "The size of `new_offbuf` is not a multiple of "
                          STRINGIFY(sizeof(T));
  }
  if (new_offbuf->get_elem<T>(0) != -1) {
    throw ValueError() << "Cannot use `new_offbuf` as an \"offsets\" buffer: "
                          "first element of this array is not -1: got "
                       << new_offbuf->get_elem<T>(0);
  }
  if (new_strbuf->size() !=
      static_cast<size_t>(abs(new_offbuf->get_elem<T>(new_nrows)) - 1)) {
    throw ValueError() << "The size of `new_strbuf` does not correspond to the"
                       << " last offset of `new_offbuff`: expected "
                       << new_strbuf->size() << ", got "
                       << abs(new_offbuf->get_elem<T>(new_nrows)) - 1;
  }
  // MemoryBuffer* t = new_offbuf->shallowcopy();
  // if (mbuf) mbuf->release();
  // mbuf = t;
  // t = new_strbuf->shallowcopy();
  // if (strbuf) strbuf->release();
  // strbuf = t;

  if (mbuf) mbuf->release();
  if (strbuf) strbuf->release();
  nrows = new_nrows;
  mbuf = new_offbuf;
  strbuf = new_strbuf;
}



template <typename T>
SType StringColumn<T>::stype() const {
  return stype_string(sizeof(T));
}

template <typename T>
size_t StringColumn<T>::elemsize() const {
  return sizeof(T);
}

template <typename T>
bool StringColumn<T>::is_fixedwidth() const {
  return false;
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
  // `mbuf` always contains one more element (-1) than number of rows
  return static_cast<int64_t>(mbuf->size() / sizeof(T)) - 1;
}

template <typename T>
char* StringColumn<T>::strdata() const {
  return static_cast<char*>(strbuf->get()) - 1;
}

template <typename T>
T* StringColumn<T>::offsets() const {
  return static_cast<T*>(mbuf->get()) + 1;
}


template <typename T>
void StringColumn<T>::reify() {
  // If our rowindex is null, then we're already done
  if (ri.isabsent()) return;

  //size_t new_offoff = static_cast<size_t>(offoff);
  size_t new_mbuf_size = (ri.zlength() + 1) * sizeof(T);
  size_t new_strbuf_size = 0;
  MemoryBuffer* new_mbuf = mbuf;
  MemoryBuffer* new_strbuf = strbuf;

  if (ri.isslice() && ri.slice_step() == 1) {
    T* data_src = offsets() + ri.slice_start();
    T off0 = abs(data_src[-1]);
    T off1 = abs(data_src[nrows - 1]);
    new_strbuf_size = static_cast<size_t>(off1 - off0);
    if (strbuf->is_readonly()) {
      new_strbuf = new MemoryMemBuf(new_strbuf_size);
      memcpy(new_strbuf->get(), strdata() + off0, new_strbuf_size);
    } else {
      memmove(new_strbuf->get(), strdata() + off0, new_strbuf_size);
    }
    if (mbuf->is_readonly()) {
      new_mbuf = new MemoryMemBuf(new_mbuf_size);
    }
    T* data_dest = static_cast<T*>(new_mbuf->get());
    data_dest[0] = -1;
    data_dest += 1;
    --off0;
    for (int64_t i = 0; i < nrows; ++i) {
      data_dest[i] = data_src[i] > 0 ? data_src[i] - off0 : data_src[i] + off0;
    }
  } else if (ri.isslice() && ri.slice_step() > 0) {
    // Special case: We can still do this in-place
    // (assuming the buffers are not read-only)
    if (mbuf->is_readonly())
      new_mbuf = new MemoryMemBuf(new_mbuf_size);
    if (strbuf->is_readonly())
      new_strbuf = new MemoryMemBuf(strbuf->size()); // We don't know the actual size yet
                                                     // but it can't be larger than this
    T step = static_cast<T>(ri.slice_step());
    T start = static_cast<T>(ri.slice_start());
    T* offs1 = offsets();
    T* offs0 = offs1 - 1;
    T* off_dest = static_cast<T*>(new_mbuf->get());
    char* str_src = strdata();
    char* str_dest = static_cast<char*>(new_strbuf->get());
    // We know that the resulting strbuf/mbuf size will be smaller, so no need to
    // worry about resizing beforehand
    *off_dest = -1;
    ++off_dest;
    T prev_off = 1;
    for (T i = 0, j = start; i < nrows; ++i, j += step) {
      if (offs1[j] > 0) {
        T off0 = abs(offs0[j]);
        T str_len = offs1[j] - off0;
        if (str_len != 0) {
          memmove(str_dest, str_src + off0, static_cast<size_t>(str_len));
          str_dest += str_len;
        }
        prev_off += str_len;
        off_dest[i] = prev_off;
      } else {
        off_dest[i] = -prev_off;
      }
    }
    new_strbuf_size = static_cast<size_t>(
        str_dest - static_cast<char*>(new_strbuf->get()));
    // Note: We can also do a special case with slice.step = 0, but we have to
    //       be careful about cases where nrows > T_MAX
  } else {
    // We have to make a copy otherwise :(
    new_mbuf = new MemoryMemBuf(new_mbuf_size);

    T* offs1 = offsets();
    T* offs0 = offs1 - 1;
    T strs_size = 0;
    ri.strided_loop(0, nrows, 1,
      [&](int64_t i) {
        if (offs1[i] > 0) {
          strs_size += offs1[i] - abs(offs0[i]);
        }
      });
    new_strbuf_size = static_cast<size_t>(strs_size);
    new_strbuf = new MemoryMemBuf(new_strbuf_size);
    T* offs_dest = static_cast<T*>(new_mbuf->get());
    offs_dest[0] = -1;
    ++offs_dest;
    char *strs_src = strdata();
    char *strs_dest = static_cast<char*>(new_strbuf->get());
    T prev_off = 1;
    ri.strided_loop(0, nrows, 1,
      [&](int64_t i) {
        if (offs1[i] > 0) {
          T off0 = abs(offs0[i]);
          T str_len = offs1[i] - off0;
          if (str_len != 0) {
            memcpy(strs_dest, strs_src + off0, static_cast<size_t>(str_len));
            strs_dest += str_len;
            prev_off += str_len;
          }
          *offs_dest = prev_off;
          ++offs_dest;
        } else {
          *offs_dest = -prev_off;
          ++offs_dest;
        }
      });
  }
  if (new_mbuf == mbuf) {
    mbuf->resize(new_mbuf_size);
  } else {
    mbuf->release();
    mbuf = new_mbuf;
  }
  if (new_strbuf == strbuf) {
    strbuf->resize(new_strbuf_size);
  } else {
    strbuf->release();
    strbuf = new_strbuf;
  }
  ri.clear(true);
}



template <typename T>
void StringColumn<T>::resize_and_fill(int64_t new_nrows)
{
  // TODO: clean this up
  int64_t old_nrows = nrows;
  int64_t diff_rows = new_nrows - old_nrows;
  if (diff_rows == 0) return;
  if (diff_rows < 0) {
    throw RuntimeError() << "Column::resize_and_fill() cannot shrink a column";
  }

  if (new_nrows > INT32_MAX && sizeof(T) == 4) {
    // TODO: instead of throwing an error, upcast the column to <int64_t>
    // This is only an issue for the case when nrows=1. Maybe we should separate
    // the two methods?
    throw ValueError() << "Nrows is too big for a str32 column: " << new_nrows;
  }

  size_t old_strbuf_size = strbuf->size();
  size_t old_mbuf_size = mbuf->size();
  size_t new_strbuf_size = old_strbuf_size;
  size_t new_mbuf_size = sizeof(T) * (static_cast<size_t>(new_nrows) + 1);
  if (old_nrows == 1) new_strbuf_size = old_strbuf_size * (size_t) new_nrows;

  // Check if we can expand offsets in-place
  if (mbuf->is_readonly()) {
    MemoryBuffer* new_mbuf = new MemoryMemBuf(new_mbuf_size);\
    memcpy(new_mbuf->get(), mbuf->get(), old_mbuf_size);
    mbuf->release();
    mbuf = new_mbuf;
  } else {
    mbuf->resize(new_mbuf_size);
  }

  // Replicate the value, or fill with NAs
  T* offsets = static_cast<T*>(mbuf->get());
  ++offsets;
  if (old_nrows == 1 && offsets[0] > 0) {
    MemoryBuffer* new_strbuf = strbuf;
    if (strbuf->is_readonly()) {
      new_strbuf = new MemoryMemBuf(new_strbuf_size);
    } else {
      new_strbuf->resize(new_strbuf_size);
    }
    char* str_src = static_cast<char*>(strbuf->get());
    char* str_dest = static_cast<char*>(new_strbuf->get());
    T src_len = static_cast<T>(old_strbuf_size);
    for (T i = 0; i < new_nrows; ++i) {
      memcpy(str_dest, str_src, old_strbuf_size);
      str_dest += old_strbuf_size;
      offsets[i] = 1 + (i + 1) * src_len;
    }
    if (new_strbuf != strbuf) {
      strbuf->release();
      strbuf = new_strbuf;
    }
  } else {
    if (old_nrows == 1) assert(old_strbuf_size == 0);
    T na = -static_cast<T>(old_strbuf_size + 1);
    set_value(offsets + nrows, &na, sizeof(T),
              static_cast<size_t>(diff_rows));
  }
  nrows = new_nrows;
  // TODO: Temporary fix. To be resolved in #301
  if (stats != nullptr) stats->reset();
}


template <typename T>
void StringColumn<T>::rbind_impl(const std::vector<const Column*>& columns,
                                 int64_t new_nrows, bool col_empty)
{
  // Determine the size of the memory to allocate
  size_t old_nrows = static_cast<size_t>(nrows);
  size_t new_strbuf_size = 0;     // size of the string data region
  if (!col_empty) {
    new_strbuf_size += strbuf->size();
  }
  for (const Column* col : columns) {
    if (col->stype() == ST_VOID) continue;
    // TODO: replace with datasize(). But: what if col is not a string?
    new_strbuf_size += static_cast<const StringColumn<T>*>(col)->strbuf->size();
  }
  size_t new_mbuf_size = sizeof(T) * (static_cast<size_t>(new_nrows) + 1);

  // Reallocate the column
  mbuf = mbuf->safe_resize(new_mbuf_size);
  strbuf = strbuf->safe_resize(new_strbuf_size);
  assert(!mbuf->is_readonly());
  assert(!strbuf->is_readonly());
  nrows = new_nrows;
  T* offs = offsets();

  // Move the original offsets
  T rows_to_fill = 0;  // how many rows need to be filled with NAs
  T curr_offset = 0;   // Current offset within string data section
  if (col_empty) {
    rows_to_fill += old_nrows;
    offs[-1] = -1;
  } else {
    offs[-1] = -1;
    curr_offset = abs(offs[old_nrows - 1]) - 1;
    offs += old_nrows;
  }
  for (const Column* col : columns) {
    if (col->stype() == ST_VOID) {
      rows_to_fill += col->nrows;
    } else {
      if (rows_to_fill) {
        const T na = -curr_offset - 1;
        set_value(offs, &na, sizeof(T), static_cast<size_t>(rows_to_fill));
        offs += rows_to_fill;
        rows_to_fill = 0;
      }
      T* col_offsets = static_cast<const StringColumn<T>*>(col)->offsets();
      int64_t col_nrows = col->nrows;
      for (int64_t j = 0; j < col_nrows; ++j) {
        T off = col_offsets[j];
        *offs++ = off > 0? off + curr_offset : off - curr_offset;
      }
      MemoryBuffer* col_strbuf = static_cast<const StringColumn<T>*>(col)->strbuf;
      memcpy(strbuf->at(curr_offset), col_strbuf->get(), col_strbuf->size());
      curr_offset += col_strbuf->size();
    }
    delete col;
  }
  if (rows_to_fill) {
    const T na = -curr_offset - 1;
    set_value(offs, &na, sizeof(T), static_cast<size_t>(rows_to_fill));
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
  // Perform a mini reify (the actual `reify` method will copy string and offset
  // data, both of which is extraneous for this method)
  strbuf = strbuf->safe_resize(0);
  size_t new_mbuf_size = sizeof(T) * (static_cast<size_t>(nrows) + 1);
  if (mbuf->is_readonly()) {
    mbuf->release();
    mbuf = new MemoryMemBuf(new_mbuf_size);
  } else {
    mbuf->resize(new_mbuf_size);
  }
  T* off_data = offsets();
  #pragma omp parallel for
  for (int64_t i = -1; i < nrows; ++i) {
    off_data[i] = -1;
  }
  ri.clear(false);
}



//------------------------------------------------------------------------------
// Stats
//------------------------------------------------------------------------------

template <typename T>
StringStats<T>* StringColumn<T>::get_stats() const {
  if (stats == nullptr) stats = new StringStats<T>();
  return static_cast<StringStats<T>*>(stats);
}

template <typename T>
CString StringColumn<T>::mode() const {
  return get_stats()->mode(this);
}

template <typename T>
PyObject* StringColumn<T>::mode_pyscalar() const {
  return string_to_py(mode());
}

template <typename T>
Column* StringColumn<T>::mode_column() const {
  CString m = mode();
  auto col = new StringColumn<T>(1);
  if (m.size >= 0) {
    col->mbuf->set_elem(1, static_cast<T>(m.size) + 1);
    col->strbuf->resize(static_cast<size_t>(m.size));
    std::memcpy(col->strbuf->get(), m.ch, static_cast<size_t>(m.size));
  } else {
    col->mbuf->set_elem(1, static_cast<T>(-1));
  }
  return col;
}




//------------------------------------------------------------------------------
// Type casts
//------------------------------------------------------------------------------

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

  if (strbuf == nullptr) {
    icc << "String data section in " << name << " is null" << end;
    return true;
  }

  size_t strdata_size = 0;
  //*_utf8 functions use unsigned char*
  const unsigned char *cdata = (unsigned char*) strdata();
  const T *str_offsets = offsets();

  // Check that the offsets section is preceded by a -1
  if (str_offsets[-1] != -1) {
    icc << "Offsets section in (string) " << name << " is not preceded by "
        << "number -1" << end;
  }

  int64_t mbuf_nrows = data_nrows();
  strdata_size = static_cast<size_t>(abs(str_offsets[mbuf_nrows - 1]) - 1);

  if (strbuf->size() != strdata_size) {
    icc << "Size of string data section in " << name << " does not correspond"
        << " to the magnitude of the final offset: size = " << strbuf->size()
        << ", expected " << strdata_size << end;
    return true;
  }

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
    else if (oj > 0 && !is_valid_utf8(cdata + lastoff,
                                      static_cast<size_t>(oj - lastoff))) {
      icc << "Invalid UTF-8 string in row " << i << " of " << name << ": "
          << repr_utf8(cdata + lastoff, cdata + oj) << end;
    }
    lastoff = std::abs(oj);
  }

  return !icc.has_errors(nerrors);
}


static std::string path_str(const std::string& path) {
  size_t f_s = path.find_last_of("/");
  size_t f_e = path.find_last_of(".");
  if (f_s == std::string::npos) f_s = 0;
  if (f_e == std::string::npos || f_e < f_s) f_e = path.length();
  std::string res(path);
  res.insert(f_e, "_str");
  return res;
}


// Explicit instantiation of the template
template class StringColumn<int32_t>;
template class StringColumn<int64_t>;
