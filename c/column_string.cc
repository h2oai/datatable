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
StringColumn<T>::StringColumn() : Column(0) {}

template <typename T>
StringColumn<T>::StringColumn(int64_t nrows_)
  : StringColumn<T>(nrows_, MemoryRange(), MemoryRange()) {}


template <typename T>
StringColumn<T>::StringColumn(int64_t n, MemoryRange&& mb, MemoryRange&& sb)
  : Column(n)
{
  size_t exp_off_size = sizeof(T) * (static_cast<size_t>(n) + 1);
  if (mb) {
    xassert(mb.size() == exp_off_size);
    xassert(mb.get_element<T>(0) == -1);
    xassert(sb.size() == static_cast<size_t>(abs(mb.get_element<T>(n)) - 1));
  } else {
    if (sb) {
      throw Error()
        << "String buffer cannot be defined when offset buffer is null";
    }
    mb = MemoryRange(exp_off_size);
    mb.set_element<T>(0, -1);
  }

  mbuf = std::move(mb);
  strbuf = std::move(sb);
}


//==============================================================================
// Initialization methods
//==============================================================================

template <typename T>
void StringColumn<T>::init_data() {
  xassert(!ri);
  mbuf = MemoryRange((static_cast<size_t>(nrows) + 1) * sizeof(T));
  mbuf.set_element<T>(0, -1);
}

template <typename T>
void StringColumn<T>::init_mmap(const std::string& filename) {
  xassert(!ri);
  strbuf = MemoryRange(0, path_str(filename));
  mbuf = MemoryRange((static_cast<size_t>(nrows) + 1) * sizeof(T), filename);
  mbuf.set_element<T>(0, -1);
}

template <typename T>
void StringColumn<T>::open_mmap(const std::string& filename) {
  xassert(!ri);

  mbuf = MemoryRange(filename);

  size_t exp_mbuf_size = sizeof(T) * (static_cast<size_t>(nrows) + 1);
  if (mbuf.size() != exp_mbuf_size) {
    throw Error() << "File \"" << filename <<
        "\" cannot be used to create a column with " << nrows <<
        " rows. Expected file size of " << exp_mbuf_size <<
        " bytes, actual size is " << mbuf.size() << " bytes";
  }

  std::string filename_str = path_str(filename);

  strbuf = MemoryRange(filename_str);
  size_t exp_strbuf_size =
      static_cast<size_t>(abs(mbuf.get_element<T>(nrows)) - 1);

  if (strbuf.size() != exp_strbuf_size) {
    size_t strbuf_size = strbuf.size();
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
  mbuf.save_to_disk(filename, strategy);
  strbuf.save_to_disk(path_str(filename), strategy);
}

template <typename T>
Column* StringColumn<T>::shallowcopy(const RowIndex& new_rowindex) const {
  Column* newcol = Column::shallowcopy(new_rowindex);
  StringColumn<T>* col = static_cast<StringColumn<T>*>(newcol);
  col->strbuf = strbuf;
  return col;
}


template <typename T>
void StringColumn<T>::replace_buffer(MemoryRange&& new_offbuf,
                                     MemoryRange&& new_strbuf)
{
  int64_t new_nrows = new_offbuf.size()/sizeof(T) - 1;
  if (new_offbuf.size() % sizeof(T)) {
    throw ValueError() << "The size of `new_offbuf` is not a multiple of "
                          STRINGIFY(sizeof(T));
  }
  if (new_offbuf.get_element<T>(0) != -1) {
    throw ValueError() << "Cannot use `new_offbuf` as an `offsets` buffer: "
                          "first element of this array is not -1: got "
                       << new_offbuf.get_element<T>(0);
  }
  size_t lastoff = static_cast<size_t>(
                      std::abs(new_offbuf.get_element<T>(new_nrows)) - 1);
  if (new_strbuf.size() != lastoff) {
    throw ValueError() << "The size of `new_strbuf` does not correspond to the"
                          " last offset of `new_offbuff`: expected "
                       << new_strbuf.size() << ", got " << lastoff;
  }
  strbuf = std::move(new_strbuf);
  mbuf = std::move(new_offbuf);
  nrows = new_nrows;
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
size_t StringColumn<T>::datasize() const{
  size_t sz = mbuf.size();
  const T* end = static_cast<const T*>(mbuf.rptr(sz));
  return static_cast<size_t>(abs(end[-1]) - 1);
}

template <typename T>
int64_t StringColumn<T>::data_nrows() const {
  // `mbuf` always contains one more element (-1) than number of rows
  return static_cast<int64_t>(mbuf.size() / sizeof(T)) - 1;
}

template <typename T>
const char* StringColumn<T>::strdata() const {
  return static_cast<const char*>(strbuf.rptr()) - 1;
}

template <typename T>
const T* StringColumn<T>::offsets() const {
  return static_cast<const T*>(mbuf.rptr()) + 1;
}

template <typename T>
T* StringColumn<T>::offsets_w() {
  return static_cast<T*>(mbuf.wptr()) + 1;
}


template <typename T>
void StringColumn<T>::reify() {
  // If our rowindex is null, then we're already done
  if (ri.isabsent()) return;
  bool simple_slice = ri.isslice() && ri.slice_step() == 1;
  bool ascending    = ri.isslice() && ri.slice_step() > 0;

  size_t new_mbuf_size = (ri.zlength() + 1) * sizeof(T);
  size_t new_strbuf_size = 0;
  MemoryRange new_strbuf(strbuf);
  MemoryRange new_mbuf(new_mbuf_size);
  T* offs_dest = static_cast<T*>(new_mbuf.wptr());
  offs_dest[0] = -1;
  offs_dest++;

  if (simple_slice) {
    const T* data_src = offsets() + ri.slice_start();
    T off0 = std::abs(data_src[-1]);
    T off1 = std::abs(data_src[nrows - 1]);
    new_strbuf_size = static_cast<size_t>(off1 - off0);
    if (!strbuf.is_writeable()) {
      new_strbuf = MemoryRange(new_strbuf_size);
      std::memcpy(new_strbuf.wptr(), strdata() + off0, new_strbuf_size);
    } else {
      std::memmove(new_strbuf.wptr(), strdata() + off0, new_strbuf_size);
    }
    --off0;
    for (int64_t i = 0; i < nrows; ++i) {
      offs_dest[i] = data_src[i] > 0 ? data_src[i] - off0 : data_src[i] + off0;
    }

  } else if (ascending) {
    // Special case: We can still do this in-place
    // (assuming the buffers are not read-only)
    if (!strbuf.is_writeable())
      new_strbuf = MemoryRange(strbuf.size()); // We don't know the actual size yet
                                               // but it can't be larger than this
    T step = static_cast<T>(ri.slice_step());
    T start = static_cast<T>(ri.slice_start());
    const T* offs1 = offsets();
    const T* offs0 = offs1 - 1;
    const char* str_src = strdata();
    char* str_dest = static_cast<char*>(new_strbuf.wptr());
    // We know that the resulting strbuf/mbuf size will be smaller, so no need to
    // worry about resizing beforehand
    T prev_off = 1;
    for (T i = 0, j = start; i < nrows; ++i, j += step) {
      if (offs1[j] > 0) {
        T off0 = std::abs(offs0[j]);
        T str_len = offs1[j] - off0;
        if (str_len != 0) {
          std::memmove(str_dest, str_src + off0, static_cast<size_t>(str_len));
          str_dest += str_len;
        }
        prev_off += str_len;
        offs_dest[i] = prev_off;
      } else {
        offs_dest[i] = -prev_off;
      }
    }
    new_strbuf_size = static_cast<size_t>(
        str_dest - static_cast<const char*>(new_strbuf.rptr()));
    // Note: We can also do a special case with slice.step = 0, but we have to
    //       be careful about cases where nrows > T_MAX
  } else {
    const T* offs1 = offsets();
    const T* offs0 = offs1 - 1;
    T strs_size = 0;
    ri.strided_loop(0, nrows, 1,
      [&](int64_t i) {
        if (offs1[i] > 0) {
          strs_size += offs1[i] - std::abs(offs0[i]);
        }
      });
    new_strbuf_size = static_cast<size_t>(strs_size);
    new_strbuf = MemoryRange(new_strbuf_size);
    const char* strs_src = strdata();
    char* strs_dest = static_cast<char*>(new_strbuf.wptr());
    T prev_off = 1;
    ri.strided_loop(0, nrows, 1,
      [&](int64_t i) {
        if (offs1[i] > 0) {
          T off0 = std::abs(offs0[i]);
          T str_len = offs1[i] - off0;
          if (str_len != 0) {
            std::memcpy(strs_dest, strs_src + off0, static_cast<size_t>(str_len));
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

  new_strbuf.resize(new_strbuf_size);
  mbuf = std::move(new_mbuf);
  strbuf = std::move(new_strbuf);
  ri.clear();
}


template <typename T>
void StringColumn<T>::replace_values(
    RowIndex /*replace_at*/, const Column* /*replace_with*/)
{
  // TODO
}


template <typename T>
void StringColumn<T>::resize_and_fill(int64_t new_nrows)
{
  // TODO: clean this up
  int64_t old_nrows = nrows;
  int64_t diff_rows = new_nrows - old_nrows;
  if (diff_rows == 0) return;

  if (new_nrows > INT32_MAX && sizeof(T) == 4) {
    // TODO: instead of throwing an error, upcast the column to <int64_t>
    // This is only an issue for the case when nrows=1. Maybe we should separate
    // the two methods?
    throw ValueError() << "Nrows is too big for a str32 column: " << new_nrows;
  }

  size_t znrows = static_cast<size_t>(new_nrows);
  size_t old_strbuf_size = strbuf.size();
  size_t new_strbuf_size = old_strbuf_size;
  size_t new_mbuf_size = sizeof(T) * (znrows + 1);
  if (old_nrows == 1) {
    new_strbuf_size = old_strbuf_size * znrows;
  }
  if (diff_rows < 0) {
    T lastoff = mbuf.get_element<T>(new_nrows + 1);
    new_strbuf_size = static_cast<size_t>(std::abs(lastoff));
  }

  // Resize the offsets buffer
  mbuf.resize(new_mbuf_size);

  if (diff_rows < 0) {
    strbuf.resize(new_strbuf_size);
  } else {
    // Replicate the value, or fill with NAs
    T* offsets = static_cast<T*>(mbuf.wptr());
    ++offsets;
    if (old_nrows == 1 && offsets[0] > 0) {
      MemoryRange new_strbuf = strbuf;
      new_strbuf.resize(new_strbuf_size);
      const char* str_src = static_cast<const char*>(strbuf.rptr());
      char* str_dest = static_cast<char*>(new_strbuf.wptr());
      T src_len = static_cast<T>(old_strbuf_size);
      for (T i = 0; i < new_nrows; ++i) {
        std::memcpy(str_dest, str_src, old_strbuf_size);
        str_dest += old_strbuf_size;
        offsets[i] = 1 + (i + 1) * src_len;
      }
      strbuf = new_strbuf;
    } else {
      if (old_nrows == 1) xassert(old_strbuf_size == 0);
      T na = -static_cast<T>(old_strbuf_size + 1);
      set_value(offsets + nrows, &na, sizeof(T),
                static_cast<size_t>(diff_rows));
    }
  }
  nrows = new_nrows;
  // TODO: Temporary fix. To be resolved in #301
  if (stats != nullptr) stats->reset();
}


template <typename T>
void StringColumn<T>::rbind_impl(std::vector<const Column*>& columns,
                                 int64_t new_nrows, bool col_empty)
{
  // Determine the size of the memory to allocate
  size_t old_nrows = static_cast<size_t>(nrows);
  size_t new_strbuf_size = 0;     // size of the string data region
  if (!col_empty) {
    new_strbuf_size += strbuf.size();
  }
  for (size_t i = 0; i < columns.size(); ++i) {
    const Column* col = columns[i];
    if (col->stype() == ST_VOID) continue;
    if (col->stype() != stype()) {
      columns[i] = col->cast(stype());
      delete col;
      col = columns[i];
    }
    // TODO: replace with datasize(). But: what if col is not a string?
    new_strbuf_size += static_cast<const StringColumn<T>*>(col)->strbuf.size();
  }
  size_t new_mbuf_size = sizeof(T) * (static_cast<size_t>(new_nrows) + 1);

  // Reallocate the column
  mbuf.resize(new_mbuf_size);
  strbuf.resize(new_strbuf_size);
  xassert(mbuf.is_writeable() && strbuf.is_writeable());
  nrows = new_nrows;
  T* offs = offsets_w();

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
      const T* col_offsets = static_cast<const StringColumn<T>*>(col)->offsets();
      int64_t col_nrows = col->nrows;
      for (int64_t j = 0; j < col_nrows; ++j) {
        T off = col_offsets[j];
        *offs++ = off > 0? off + curr_offset : off - curr_offset;
      }
      const MemoryRange& col_strbuf = static_cast<const StringColumn<T>*>(col)->strbuf;
      void* target = strbuf.wptr(static_cast<size_t>(curr_offset));
      std::memcpy(target, col_strbuf.rptr(), col_strbuf.size());
      curr_offset += col_strbuf.size();
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
  const int8_t* maskdata = mask->elements_r();
  char* strdata = static_cast<char*>(strbuf.wptr()) - 1;
  T* offsets = this->offsets_w();

  // How much to reduce the offsets by due to some strings turning into NAs
  T doffset = 0;
  T offp = 1;
  for (int64_t j = 0; j < nrows; ++j) {
    T offi = offsets[j];
    T offa = std::abs(offi);
    if (maskdata[j] == 1) {
      doffset += offa - offp;
      offsets[j] = -offp;
      continue;
    } else if (doffset) {
      if (offi > 0) {
        offsets[j] = offi - doffset;
        std::memmove(strdata + offp, strdata + offp + doffset,
                     static_cast<size_t>(offi - offp - doffset));
      } else {
        offsets[j] = -offp;
      }
    }
    offp = offa;
  }
  if (stats) stats->reset();
}

template <typename T>
void StringColumn<T>::fill_na() {
  // Perform a mini reify (the actual `reify` method will copy string and offset
  // data, both of which is extraneous for this method)
  strbuf.resize(0);
  size_t new_mbuf_size = sizeof(T) * (static_cast<size_t>(nrows) + 1);
  mbuf.resize(new_mbuf_size, /* keep_data = */ false);
  T* off_data = offsets_w();
  #pragma omp parallel for
  for (int64_t i = -1; i < nrows; ++i) {
    off_data[i] = -1;
  }
  ri.clear();
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
    col->mbuf.set_element(1, static_cast<T>(m.size) + 1);
    col->strbuf.resize(static_cast<size_t>(m.size));
    std::memcpy(col->strbuf.wptr(), m.ch, static_cast<size_t>(m.size));
  } else {
    col->mbuf.set_element(1, static_cast<T>(-1));
  }
  return col;
}




//------------------------------------------------------------------------------
// Type casts
//------------------------------------------------------------------------------

template <typename T>
void StringColumn<T>::cast_into(PyObjectColumn* target) const {
  const char* strdata = this->strdata();
  const T* offsets = this->offsets();
  PyObject** trg_data = target->elements_w();

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


template <>
void StringColumn<int32_t>::cast_into(StringColumn<int64_t>* target) const {
  const int32_t* src_data = this->offsets() - 1;
  int64_t* trg_data = target->offsets_w() - 1;
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i <= this->nrows; ++i) {
    trg_data[i] = static_cast<int32_t>(src_data[i]);
  }
  target->replace_buffer(target->data_buf(), MemoryRange(strbuf));
}

template <>
void StringColumn<int64_t>::cast_into(StringColumn<int64_t>* target) const {
  size_t alloc_size = sizeof(int64_t) * static_cast<size_t>(1 + this->nrows);
  std::memcpy(target->data_w(), this->data(), alloc_size);
  target->replace_buffer(target->data_buf(), MemoryRange(strbuf));
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

  size_t strdata_size = 0;
  //*_utf8 functions use unsigned char*
  const uint8_t* cdata = reinterpret_cast<const uint8_t*>(strdata());
  const T* str_offsets = offsets();

  // Check that the offsets section is preceded by a -1
  if (str_offsets[-1] != -1) {
    icc << "Offsets section in (string) " << name << " is not preceded by "
        << "number -1" << end;
  }

  int64_t mbuf_nrows = data_nrows();
  strdata_size = static_cast<size_t>(abs(str_offsets[mbuf_nrows - 1]) - 1);

  if (strbuf.size() != strdata_size) {
    icc << "Size of string data section in " << name << " does not correspond"
        << " to the magnitude of the final offset: size = " << strbuf.size()
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
