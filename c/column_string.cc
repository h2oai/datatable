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
#include "encodings.h"
#include "python/string.h"
#include "py_utils.h"
#include "utils.h"
#include "utils/assert.h"
#include "utils/parallel.h"

// Returns the expected path of the string data file given
// the path to the offsets
static std::string path_str(const std::string& path);

template <typename T>
StringColumn<T>::StringColumn() : Column(0) {}

template <typename T>
StringColumn<T>::StringColumn(size_t nrows_)
  : StringColumn<T>(nrows_, MemoryRange(), MemoryRange()) {}


template <typename T>
StringColumn<T>::StringColumn(size_t n, MemoryRange&& mb, MemoryRange&& sb)
  : Column(n)
{
  size_t exp_off_size = sizeof(T) * (n + 1);
  if (mb) {
    xassert(mb.size() == exp_off_size);
    xassert(mb.get_element<T>(0) == 0);
    xassert(sb.size() == (mb.get_element<T>(n) & ~GETNA<T>()));
  } else {
    xassert(!sb);
    mb = MemoryRange::mem(exp_off_size);
    mb.set_element<T>(0, 0);
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
  mbuf = MemoryRange::mem((nrows + 1) * sizeof(T));
  mbuf.set_element<T>(0, 0);
}

template <typename T>
void StringColumn<T>::init_mmap(const std::string& filename) {
  xassert(!ri);
  size_t mbuf_size = (nrows + 1) * sizeof(T);
  strbuf = MemoryRange::mmap(path_str(filename), 0);
  mbuf = MemoryRange::mmap(filename, mbuf_size);
  mbuf.set_element<T>(0, 0);
}

template <typename T>
void StringColumn<T>::open_mmap(const std::string& filename, bool recode) {
  xassert(!ri);
  std::string filename_str = path_str(filename);

  mbuf = MemoryRange::mmap(filename);
  strbuf = MemoryRange::mmap(filename_str);

  // size_t exp_mbuf_size = sizeof(T) * (nrows + 1);
  // if (mbuf.size() != exp_mbuf_size) {
  //   throw Error() << "File \"" << filename <<
  //       "\" cannot be used to create a column with " << nrows <<
  //       " rows. Expected file size of " << exp_mbuf_size <<
  //       " bytes, actual size is " << mbuf.size() << " bytes";
  // }
  if (recode) {
    if (mbuf.get_element<T>(0) != 0) {
      // Recode old format of string storage
      T* offsets = static_cast<T*>(mbuf.wptr()) + 1;
      offsets[-1] = 0;
      for (size_t i = 0; i < nrows; ++i) {
        T x = offsets[i];
        offsets[i] = ISNA<T>(x)? GETNA<T>() - x - 1 : x - 1;
      }
    }
  }

  // size_t exp_strbuf_size = (mbuf.get_element<T>(nrows) & ~GETNA<T>());
  // if (strbuf.size() != exp_strbuf_size) {
  //   size_t strbuf_size = strbuf.size();
  //   throw Error() << "File \"" << filename_str <<
  //     "\" cannot be used to create a column with " << nrows <<
  //     " rows. Expected file size of " << exp_strbuf_size <<
  //     " bytes, actual size is " << strbuf_size << " bytes";
  // }
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
  size_t new_nrows = new_offbuf.size()/sizeof(T) - 1;
  if (new_offbuf.size() % sizeof(T)) {
    throw ValueError() << "The size of `new_offbuf` is not a multiple of "
                          STRINGIFY(sizeof(T));
  }
  if (new_offbuf.get_element<T>(0) != 0) {
    throw ValueError() << "Cannot use `new_offbuf` as an `offsets` buffer: "
                          "first element of this array is not 0: got "
                       << new_offbuf.get_element<T>(0);
  }
  size_t lastoff = new_offbuf.get_element<T>(new_nrows) & ~GETNA<T>();
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
SType StringColumn<T>::stype() const noexcept {
  return sizeof(T) == 4? SType::STR32 :
         sizeof(T) == 8? SType::STR64 : SType::VOID;
}

template <typename T>
py::oobj StringColumn<T>::get_value_at_index(size_t i) const {
  size_t j = (this->ri)[i];
  const T* offs = this->offsets();
  T off_end = offs[j];
  if (ISNA<T>(off_end)) return py::None();
  T off_beg = offs[j - 1] & ~GETNA<T>();
  return py::ostring(this->strdata() + off_beg,
                     static_cast<size_t>(off_end - off_beg));
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
  return static_cast<size_t>(end[-1] & ~GETNA<T>());
}

template <typename T>
size_t StringColumn<T>::data_nrows() const {
  // `mbuf` always contains one more element than the number of rows
  return mbuf.size() / sizeof(T) - 1;
}

template <typename T>
const char* StringColumn<T>::strdata() const {
  return static_cast<const char*>(strbuf.rptr());
}

template <typename T>
const uint8_t* StringColumn<T>::ustrdata() const {
  return static_cast<const uint8_t*>(strbuf.rptr());
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
  bool ascending_slice = ri.isslice() &&
                         static_cast<int64_t>(ri.slice_step()) > 0;

  size_t new_mbuf_size = (ri.size() + 1) * sizeof(T);
  size_t new_strbuf_size = 0;
  MemoryRange new_strbuf = strbuf;
  MemoryRange new_mbuf = MemoryRange::mem(new_mbuf_size);
  T* offs_dest = static_cast<T*>(new_mbuf.wptr()) + 1;
  offs_dest[-1] = 0;

  if (simple_slice) {
    const T* data_src = offsets() + ri.slice_start();
    T off0 = data_src[-1] & ~GETNA<T>();
    T off1 = data_src[nrows - 1] & ~GETNA<T>();
    new_strbuf_size = static_cast<size_t>(off1 - off0);
    if (!strbuf.is_writable()) {
      new_strbuf = MemoryRange::mem(new_strbuf_size);
      std::memcpy(new_strbuf.wptr(), strdata() + off0, new_strbuf_size);
    } else {
      std::memmove(new_strbuf.wptr(), strdata() + off0, new_strbuf_size);
    }
    for (size_t i = 0; i < nrows; ++i) {
      offs_dest[i] = data_src[i] - off0;
    }

  } else if (ascending_slice) {
    // Special case: We can still do this in-place
    // (assuming the buffers are not read-only)
    if (!strbuf.is_writable())
      new_strbuf = MemoryRange::mem(strbuf.size()); // We don't know the actual size yet
                                                    // but it can't be larger than this
    size_t step = ri.slice_step();
    size_t start = ri.slice_start();
    const T* offs1 = offsets();
    const T* offs0 = offs1 - 1;
    const char* str_src = strdata();
    char* str_dest = static_cast<char*>(new_strbuf.wptr());
    // We know that the resulting strbuf/mbuf size will be smaller, so no need to
    // worry about resizing beforehand
    T prev_off = 0;
    size_t j = start;
    for (size_t i = 0; i < nrows; ++i, j += step) {
      if (ISNA<T>(offs1[j])) {
        offs_dest[i] = prev_off | GETNA<T>();
      } else {
        T off0 = offs0[j] & ~GETNA<T>();
        T str_len = offs1[j] - off0;
        if (str_len != 0) {
          std::memmove(str_dest, str_src + off0, static_cast<size_t>(str_len));
          str_dest += str_len;
        }
        prev_off += str_len;
        offs_dest[i] = prev_off;
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
    ri.iterate(0, nrows, 1,
      [&](size_t, size_t j) {
        if (j == RowIndex::NA) return;
        strs_size += offs1[j] - offs0[j];
      });
    strs_size &= ~GETNA<T>();
    new_strbuf_size = static_cast<size_t>(strs_size);
    new_strbuf = MemoryRange::mem(new_strbuf_size);
    const char* strs_src = strdata();
    char* strs_dest = static_cast<char*>(new_strbuf.wptr());
    T prev_off = 0;
    ri.iterate(0, nrows, 1,
      [&](size_t i, size_t j) {
        if (j == RowIndex::NA || ISNA<T>(offs1[j])) {
          offs_dest[i] = prev_off | GETNA<T>();
        } else {
          T off0 = offs0[j] & ~GETNA<T>();
          T str_len = offs1[j] - off0;
          if (str_len != 0) {
            std::memcpy(strs_dest + prev_off, strs_src + off0, str_len);
            prev_off += str_len;
          }
          offs_dest[i] = prev_off;
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
    RowIndex replace_at, const Column* replace_with)
{
  reify();
  Column* rescol = nullptr;

  if (replace_with && replace_with->stype() != stype()){
    replace_with = replace_with->cast(stype());
  }
  // This could be nullptr too
  auto repl_col = static_cast<const StringColumn<T>*>(replace_with);

  if (!replace_with || replace_with->nrows == 1) {
    CString repl_value;  // Default constructor creates an NA string
    if (replace_with) {
      T off0 = repl_col->offsets()[0];
      if (!ISNA<T>(off0)) {
        repl_value = CString(repl_col->strdata(), static_cast<int64_t>(off0));
      }
    }
    MemoryRange mask = replace_at.as_boolean_mask(nrows);
    auto mask_indices = static_cast<const int8_t*>(mask.rptr());
    rescol = dt::map_str2str(this,
      [=](size_t i, CString& value, dt::fhbuf& sb) {
        sb.write(mask_indices[i]? repl_value : value);
      });
  }
  else {
    const char* repl_strdata = repl_col->strdata();
    const T* repl_offsets = repl_col->offsets();

    MemoryRange mask = replace_at.as_integer_mask(nrows);
    auto mask_indices = static_cast<const int32_t*>(mask.rptr());
    rescol = dt::map_str2str(this,
      [=](size_t i, CString& value, dt::fhbuf& sb) {
        int ir = mask_indices[i];
        if (ir == -1) {
          sb.write(value);
        } else {
          T offstart = repl_offsets[ir - 1] & ~GETNA<T>();
          T offend = repl_offsets[ir];
          if (ISNA<T>(offend)) {
            sb.write_na();
          } else {
            sb.write(repl_strdata + offstart, offend - offstart);
          }
        }
      });
  }

  xassert(rescol);
  StringColumn<T>* scol = static_cast<StringColumn<T>*>(rescol);
  std::swap(mbuf, scol->mbuf);
  std::swap(strbuf, scol->strbuf);
  delete rescol;
  if (stats) stats->reset();
}


template <typename T>
void StringColumn<T>::resize_and_fill(size_t new_nrows)
{
  size_t old_nrows = nrows;
  if (new_nrows == old_nrows) return;
  reify();

  if (new_nrows > INT32_MAX && sizeof(T) == 4) {
    // TODO: instead of throwing an error, upcast the column to <uint64_t>
    // This is only an issue for the case when nrows=1. Maybe we should separate
    // the two methods?
    throw ValueError() << "Nrows is too big for a str32 column: " << new_nrows;
  }

  size_t old_strbuf_size = strbuf.size();
  size_t new_strbuf_size = old_strbuf_size;
  size_t new_mbuf_size = sizeof(T) * (new_nrows + 1);
  if (old_nrows == 1) {
    new_strbuf_size = old_strbuf_size * new_nrows;
  }
  if (new_nrows < old_nrows) {
    T lastoff = mbuf.get_element<T>(new_nrows);
    new_strbuf_size = static_cast<size_t>(lastoff & ~GETNA<T>());
  }

  // Resize the offsets buffer
  mbuf.resize(new_mbuf_size);

  if (new_nrows < old_nrows) {
    strbuf.resize(new_strbuf_size);
  } else {
    // Replicate the value, or fill with NAs
    T* offsets = static_cast<T*>(mbuf.wptr()) + 1;
    if (old_nrows == 1 && !ISNA<T>(offsets[0])) {
      MemoryRange new_strbuf = MemoryRange::mem(new_strbuf_size);
      const char* str_src = static_cast<const char*>(strbuf.rptr());
      char* str_dest = static_cast<char*>(new_strbuf.wptr());
      T src_len = static_cast<T>(old_strbuf_size);
      for (size_t i = 0; i < new_nrows; ++i) {
        std::memcpy(str_dest, str_src, old_strbuf_size);
        str_dest += old_strbuf_size;
        offsets[i] = static_cast<T>(i + 1) * src_len;
      }
      strbuf = new_strbuf;
    } else {
      if (old_nrows == 1) xassert(old_strbuf_size == 0);
      T na = static_cast<T>(old_strbuf_size) | GETNA<T>();
      set_value(offsets + nrows, &na, sizeof(T), new_nrows - old_nrows);
    }
  }
  nrows = new_nrows;
  // TODO: Temporary fix. To be resolved in #301
  if (stats != nullptr) stats->reset();
}


template <typename T>
void StringColumn<T>::rbind_impl(std::vector<const Column*>& columns,
                                 size_t new_nrows, bool col_empty)
{
  // Determine the size of the memory to allocate
  size_t old_nrows = nrows;
  size_t new_strbuf_size = 0;     // size of the string data region
  if (!col_empty) {
    new_strbuf_size += strbuf.size();
  }
  for (size_t i = 0; i < columns.size(); ++i) {
    const Column* col = columns[i];
    if (col->stype() == SType::VOID) continue;
    if (col->stype() != stype()) {
      columns[i] = col->cast(stype());
      delete col;
      col = columns[i];
    }
    // TODO: replace with datasize(). But: what if col is not a string?
    new_strbuf_size += static_cast<const StringColumn<T>*>(col)->strbuf.size();
  }
  size_t new_mbuf_size = sizeof(T) * (new_nrows + 1);

  // Reallocate the column
  mbuf.resize(new_mbuf_size);
  strbuf.resize(new_strbuf_size);
  nrows = new_nrows;
  T* offs = offsets_w();

  // Move the original offsets
  size_t rows_to_fill = 0;  // how many rows need to be filled with NAs
  T curr_offset = 0;        // Current offset within string data section
  offs[-1] = 0;
  if (col_empty) {
    rows_to_fill += old_nrows;
  } else {
    curr_offset = offs[old_nrows - 1] & ~GETNA<T>();
    offs += old_nrows;
  }
  for (const Column* col : columns) {
    if (col->stype() == SType::VOID) {
      rows_to_fill += col->nrows;
    } else {
      if (rows_to_fill) {
        const T na = curr_offset | GETNA<T>();
        set_value(offs, &na, sizeof(T), rows_to_fill);
        offs += rows_to_fill;
        rows_to_fill = 0;
      }
      const T* col_offsets = static_cast<const StringColumn<T>*>(col)->offsets();
      size_t col_nrows = col->nrows;
      for (size_t j = 0; j < col_nrows; ++j) {
        T off = col_offsets[j];
        *offs++ = off + curr_offset;
      }
      auto strcol = static_cast<const StringColumn<T>*>(col);
      size_t sz = strcol->strbuf.size();
      if (sz) {
        void* target = strbuf.wptr(static_cast<size_t>(curr_offset));
        std::memcpy(target, strcol->strbuf.rptr(), sz);
        curr_offset += sz;
      }
    }
    delete col;
  }
  if (rows_to_fill) {
    const T na = curr_offset | GETNA<T>();
    set_value(offs, &na, sizeof(T), rows_to_fill);
  }
}


template <typename T>
void StringColumn<T>::apply_na_mask(const BoolColumn* mask) {
  const int8_t* maskdata = mask->elements_r();
  char* strdata = static_cast<char*>(strbuf.wptr());
  T* offsets = this->offsets_w();

  // How much to reduce the offsets1 by due to some strings turning into NAs
  T doffset = 0;
  T offp = 0;
  for (size_t j = 0; j < nrows; ++j) {
    T offi = offsets[j];
    T offa = offi & ~GETNA<T>();
    if (maskdata[j] == 1) {
      doffset += offa - offp;
      offsets[j] = offp | GETNA<T>();
      continue;
    } else if (doffset) {
      if (offi == offa) {
        offsets[j] = offi - doffset;
        std::memmove(strdata + offp, strdata + offp + doffset,
                     offi - offp - doffset);
      } else {
        offsets[j] = offp | GETNA<T>();
      }
    }
    offp = offa;
  }
  if (stats) stats->reset();
}

template <typename T>
void StringColumn<T>::fill_na() {
  // Perform a mini reify (the actual `reify` method will copy string and offset
  // data, both of which are extraneous for this method)
  strbuf.resize(0);
  size_t new_mbuf_size = sizeof(T) * (nrows + 1);
  mbuf.resize(new_mbuf_size, /* keep_data = */ false);
  T* off_data = offsets_w();
  off_data[-1] = 0;
  #pragma omp parallel for
  for (size_t i = 0; i < nrows; ++i) {
    off_data[i] = GETNA<T>();
  }
  ri.clear();
}


// Compare 2 strings, each is given as a string buffer + offsets to each
// strings' start and end.
// Return -1 if str1 < str2; 0 if str1 == str2; and 1 if str1 > str2.
// An NA string compares equal to another NA string, but < than any other
// non-NA string.
// Note: this function presumes that `start1` and `start2` were already
// cleared from the NA flag.
//
template <typename T>
int compare_strings(const uint8_t* strdata1, T start1, T end1,
                    const uint8_t* strdata2, T start2, T end2)
{
  if (ISNA<T>(end1)) return ISNA<T>(end2) - 1;
  if (ISNA<T>(end2)) return 1;

  T len1 = end1 - start1;
  T len2 = end2 - start2;
  strdata1 += start1;
  strdata2 += start2;
  for (T i = 0; i < len1; ++i) {
    if (i == len2) return 1;  // str2 is shorter than str1
    uint8_t ch1 = strdata1[i];
    uint8_t ch2 = strdata2[i];
    if (ch1 != ch2) {
      return 1 - 2*(ch1 < ch2);
    }
  }
  return -(len1 != len2);
}


template <typename T>
static int32_t binsearch(const uint8_t* strdata, const T* offsets, uint32_t len,
                         const uint8_t* src, T ostart, T oend)
{
  ostart &= ~GETNA<T>();
  // Use unsigned indices in order to avoid overflows. LOL about -1
  uint32_t start = -1U;
  uint32_t end   = len;
  const T* start_offsets = offsets - 1;
  while (end - start > 1) {
    uint32_t mid = (start + end) >> 1;
    T vstart = start_offsets[mid] & ~GETNA<T>();
    T vend = offsets[mid];
    int cmp = compare_strings<T>(strdata, vstart, vend, src, ostart, oend);
    if (cmp < 0) {  // mid < o
      start = mid;
    } else if (cmp > 0) {
      end = mid;
    } else {
      return static_cast<int32_t>(mid);
    }
  }
  return -1;
}


template <typename T>
RowIndex StringColumn<T>::join(const Column* keycol) const {
  xassert(stype() == keycol->stype());

  auto kcol = static_cast<const StringColumn<T>*>(keycol);
  xassert(!kcol->ri);

  arr32_t target_indices(nrows);
  int32_t* trg_indices = target_indices.data();
  const T* src_offsets = offsets();
  const T* key_offsets = kcol->offsets();
  const uint8_t* src_strdata = ustrdata();
  const uint8_t* key_strdata = kcol->ustrdata();
  uint32_t key_n = static_cast<uint32_t>(keycol->nrows);

  ri.iterate(0, nrows, 1,
    [&](size_t i, size_t j) {
      if (j == RowIndex::NA) return;
      T ostart = src_offsets[j - 1];
      T oend = src_offsets[j];
      trg_indices[i] = binsearch<T>(key_strdata, key_offsets, key_n,
                                    src_strdata, ostart, oend);
    });

  return RowIndex(std::move(target_indices));
}


template <typename T>
void StringColumn<T>::fill_na_mask(int8_t* outmask, size_t row0, size_t row1) {
  const T* offs = this->offsets();
  for (size_t i = row0; i < row1; ++i) {
    outmask[i] = ISNA<T>(offs[i]);
  }
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
    col->mbuf.set_element(1, static_cast<T>(m.size));
    col->strbuf.resize(static_cast<size_t>(m.size));
    std::memcpy(col->strbuf.wptr(), m.ch, static_cast<size_t>(m.size));
  } else {
    col->mbuf.set_element(1, GETNA<T>());
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

  T prev_offset = 0;
  for (size_t i = 0; i < this->nrows; ++i) {
    T off_end = offsets[i];
    if (ISNA<T>(off_end)) {
      trg_data[i] = none();
    } else {
      auto len = static_cast<Py_ssize_t>(off_end - prev_offset);
      trg_data[i] = PyUnicode_FromStringAndSize(strdata + prev_offset, len);
      prev_offset = off_end;
    }
  }
}


template <>
void StringColumn<uint32_t>::cast_into(StringColumn<uint64_t>* target) const {
  const uint32_t* src_data = this->offsets();
  uint64_t* trg_data = target->offsets_w();
  uint64_t dNA = GETNA<uint64_t>() - GETNA<uint32_t>();
  trg_data[-1] = 0;
  #pragma omp parallel for schedule(static)
  for (size_t i = 0; i < this->nrows; ++i) {
    uint32_t v = src_data[i];
    trg_data[i] = ISNA<uint32_t>(v)? v + dNA : v;
  }
  target->replace_buffer(target->data_buf(), MemoryRange(strbuf));
}

template <>
void StringColumn<uint64_t>::cast_into(StringColumn<uint64_t>* target) const {
  size_t alloc_size = sizeof(uint64_t) * (1 + this->nrows);
  std::memcpy(target->data_w(), this->data(), alloc_size);
  target->replace_buffer(target->data_buf(), MemoryRange(strbuf));
}



//------------------------------------------------------------------------------
// Integrity checks
//------------------------------------------------------------------------------

template <typename T>
void StringColumn<T>::verify_integrity(const std::string& name) const {
  Column::verify_integrity(name);

  size_t strdata_size = 0;
  //*_utf8 functions use unsigned char*
  const uint8_t* cdata = reinterpret_cast<const uint8_t*>(strdata());
  const T* str_offsets = offsets();

  // Check that the offsets section is preceded by a -1
  if (str_offsets[-1] != 0) {
    throw AssertionError()
        << "Offsets section in (string) " << name << " does not start with 0";
  }

  size_t mbuf_nrows = data_nrows();
  strdata_size = str_offsets[mbuf_nrows - 1] & ~GETNA<T>();

  if (strbuf.size() != strdata_size) {
    throw AssertionError()
        << "Size of string data section in " << name << " does not correspond"
           " to the magnitude of the final offset: size = " << strbuf.size()
        << ", expected " << strdata_size;
  }

  // Check for the validity of each offset
  T lastoff = 0;
  for (size_t i = 0; i < mbuf_nrows; ++i) {
    T oj = str_offsets[i];
    if (ISNA<T>(oj)) {
      if (oj != (lastoff | GETNA<T>())) {
        throw AssertionError()
            << "Offset of NA String in row " << i << " of " << name
            << " does not have the same magnitude as the previous offset: "
               "offset = " << oj << ", previous offset = " << lastoff;
      }
    } else {
      if (oj < lastoff) {
        throw AssertionError()
            << "String offset in row " << i << " of " << name
            << " cannot be less than the previous offset: offset = " << oj
            << ", previous offset = " << lastoff;
      }
      if (!is_valid_utf8(cdata + lastoff, static_cast<size_t>(oj - lastoff))) {
        throw AssertionError()
            << "Invalid UTF-8 string in row " << i << " of " << name << ": "
            << repr_utf8(cdata + lastoff, cdata + oj);
      }
      lastoff = oj;
    }
  }
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
template class StringColumn<uint32_t>;
template class StringColumn<uint64_t>;
