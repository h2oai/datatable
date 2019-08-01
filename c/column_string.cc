//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "parallel/api.h"           // dt::parallel_for_static
#include "parallel/string_utils.h"  // dt::map_str2str
#include "python/string.h"
#include "utils/assert.h"
#include "utils/misc.h"
#include "column.h"

template <typename T> constexpr SType stype_for() { return SType::VOID; }
template <> constexpr SType stype_for<uint32_t>() { return SType::STR32; }
template <> constexpr SType stype_for<uint64_t>() { return SType::STR64; }


//------------------------------------------------------------------------------
// String column construction
//------------------------------------------------------------------------------

// public: create a string column for `n` rows, preallocating the offsets array
// but leaving string buffer empty (and not allocated).
template <typename T>
StringColumn<T>::StringColumn(size_t n) : Column(n)
{
  _stype = stype_for<T>();
  mbuf = MemoryRange::mem(sizeof(T) * (n + 1));
  mbuf.set_element<T>(0, 0);
}


// private use only
template <typename T>
StringColumn<T>::StringColumn() : Column(0) {
  _stype = stype_for<T>();
}


// private: use `new_string_column(n, &&mb, &&sb)` instead
template <typename T>
StringColumn<T>::StringColumn(size_t n, MemoryRange&& mb, MemoryRange&& sb)
  : Column(n)
{
  xassert(mb);
  xassert(mb.size() == sizeof(T) * (n + 1));
  xassert(mb.get_element<T>(0) == 0);
  xassert(sb.size() == (mb.get_element<T>(n) & ~GETNA<T>()));
  _stype = stype_for<T>();
  mbuf = std::move(mb);
  strbuf = std::move(sb);
}


static MemoryRange _recode_offsets_to_u64(const MemoryRange& offsets) {
  // TODO: make this parallel
  MemoryRange off64 = MemoryRange::mem(offsets.size() * 2);
  auto data64 = static_cast<uint64_t*>(off64.xptr());
  auto data32 = static_cast<const uint32_t*>(offsets.rptr());
  data64[0] = 0;
  uint64_t curr_offset = 0;
  size_t n = offsets.size() / sizeof(uint32_t) - 1;
  for (size_t i = 1; i <= n; ++i) {
    uint32_t len = data32[i] - data32[i - 1];
    if (len == GETNA<uint32_t>()) {
      data64[i] = curr_offset ^ GETNA<uint64_t>();
    } else {
      curr_offset += len & ~GETNA<uint32_t>();
      data64[i] = curr_offset;
    }
  }
  return off64;
}


OColumn new_string_column(size_t n, MemoryRange&& data, MemoryRange&& str) {
  size_t data_size = data.size();
  size_t strb_size = str.size();

  if (data_size == sizeof(uint32_t) * (n + 1)) {
    if (strb_size <= Column::MAX_STR32_BUFFER_SIZE &&
        n <= Column::MAX_STR32_NROWS) {
      return OColumn(new StringColumn<uint32_t>(n, std::move(data), std::move(str)));
    }
    // Otherwise, offsets need to be recoded into a uint64_t array
    data = _recode_offsets_to_u64(data);
  }
  return OColumn(new StringColumn<uint64_t>(n, std::move(data), std::move(str)));
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



//==============================================================================

template <typename T>
Column* StringColumn<T>::shallowcopy() const {
  Column* newcol = Column::shallowcopy();
  StringColumn<T>* col = static_cast<StringColumn<T>*>(newcol);
  col->strbuf = strbuf;
  return col;
}


template <typename T>
bool StringColumn<T>::get_element(size_t i, CString* out) const {
  size_t j = (this->ri)[i];
  if (j == RowIndex::NA) return true;
  const T* offs = this->offsets();
  T off_end = offs[j];
  if (ISNA<T>(off_end)) return true;
  T off_beg = offs[j - 1] & ~GETNA<T>();
  out->ch = this->strdata() + off_beg,
  out->size = static_cast<int64_t>(off_end - off_beg);
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
void StringColumn<T>::materialize() {
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
        offs_dest[i] = prev_off ^ GETNA<T>();
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
          offs_dest[i] = prev_off ^ GETNA<T>();
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
    RowIndex replace_at, const OColumn& replace_with)
{
  materialize();
  OColumn rescol;

  OColumn with;
  if (replace_with) {
    with = replace_with;  // copy
    if (with.stype() != _stype) with = with->cast(_stype);
  }
  // This could be nullptr too
  auto repl_col = static_cast<const StringColumn<T>*>(with.get());

  if (!with || with.nrows() == 1) {
    CString repl_value;  // Default constructor creates an NA string
    if (with) {
      bool r = with.get_element(0, &repl_value);
      if (r) repl_value = CString();
    }
    MemoryRange mask = replace_at.as_boolean_mask(nrows);
    auto mask_indices = static_cast<const int8_t*>(mask.rptr());
    rescol = dt::map_str2str(this,
      [=](size_t i, CString& value, dt::string_buf* sb) {
        sb->write(mask_indices[i]? repl_value : value);
      });
  }
  else {
    const char* repl_strdata = repl_col->strdata();
    const T* repl_offsets = repl_col->offsets();

    MemoryRange mask = replace_at.as_integer_mask(nrows);
    auto mask_indices = static_cast<const int32_t*>(mask.rptr());
    rescol = dt::map_str2str(this,
      [=](size_t i, CString& value, dt::string_buf* sb) {
        int ir = mask_indices[i];
        if (ir == -1) {
          sb->write(value);
        } else {
          T offstart = repl_offsets[ir - 1] & ~GETNA<T>();
          T offend = repl_offsets[ir];
          if (ISNA<T>(offend)) {
            sb->write_na();
          } else {
            sb->write(repl_strdata + offstart, offend - offstart);
          }
        }
      });
  }

  xassert(rescol);
  if (rescol.stype() != _stype) {
    throw NotImplError() << "When replacing string values, the size of the "
      "resulting column exceeds the maximum for str32";
  }
  auto scol = static_cast<StringColumn<T>*>(const_cast<Column*>(rescol.get()));
  std::swap(mbuf, scol->mbuf);
  std::swap(strbuf, scol->strbuf);
  if (stats) stats->reset();
}


template <typename T>
void StringColumn<T>::resize_and_fill(size_t new_nrows)
{
  size_t old_nrows = nrows;
  if (new_nrows == old_nrows) return;
  materialize();

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
      T na = static_cast<T>(old_strbuf_size) ^ GETNA<T>();
      set_value(offsets + nrows, &na, sizeof(T), new_nrows - old_nrows);
    }
  }
  nrows = new_nrows;
  // TODO: Temporary fix. To be resolved in #301
  if (stats != nullptr) stats->reset();
}



template <typename T>
void StringColumn<T>::apply_na_mask(const OColumn& mask) {
  xassert(mask.stype() == SType::BOOL);
  auto maskdata = static_cast<const int8_t*>(mask->data());
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
      offsets[j] = offp ^ GETNA<T>();
      continue;
    } else if (doffset) {
      if (offi == offa) {
        offsets[j] = offi - doffset;
        std::memmove(strdata + offp, strdata + offp + doffset,
                     offi - offp - doffset);
      } else {
        offsets[j] = offp ^ GETNA<T>();
      }
    }
    offp = offa;
  }
  if (stats) stats->reset();
}

template <typename T>
void StringColumn<T>::fill_na() {
  xassert(!ri);
  // Perform a mini materialize (the actual `materialize` method will copy string and offset
  // data, both of which are extraneous for this method)
  strbuf.resize(0);
  size_t new_mbuf_size = sizeof(T) * (nrows + 1);
  mbuf.resize(new_mbuf_size, /* keep_data = */ false);
  T* off_data = offsets_w();
  off_data[-1] = 0;

  dt::parallel_for_static(nrows,
    [=](size_t i){
      off_data[i] = GETNA<T>();
    });
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
RowIndex StringColumn<T>::join(const OColumn& keycol) const {
  xassert(_stype == keycol.stype());
  xassert(!keycol->rowindex());

  auto kcol = static_cast<const StringColumn<T>*>(keycol.get());

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




//------------------------------------------------------------------------------
// Explicit instantiation of the template
//------------------------------------------------------------------------------

template class StringColumn<uint32_t>;
template class StringColumn<uint64_t>;
