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
#include "column_impl.h"

template <typename T> constexpr SType stype_for() { return SType::VOID; }
template <> constexpr SType stype_for<uint32_t>() { return SType::STR32; }
template <> constexpr SType stype_for<uint64_t>() { return SType::STR64; }


//------------------------------------------------------------------------------
// String column construction
//------------------------------------------------------------------------------

// public: create a string column for `n` rows, preallocating the offsets array
// but leaving string buffer empty (and not allocated).
template <typename T>
StringColumn<T>::StringColumn(size_t n) : ColumnImpl(n)
{
  _stype = stype_for<T>();
  mbuf = MemoryRange::mem(sizeof(T) * (n + 1));
  mbuf.set_element<T>(0, 0);
}


// private use only
template <typename T>
StringColumn<T>::StringColumn() : ColumnImpl(0) {
  _stype = stype_for<T>();
}


// private: use `new_string_column(n, &&mb, &&sb)` instead
template <typename T>
StringColumn<T>::StringColumn(size_t n, MemoryRange&& mb, MemoryRange&& sb)
  : ColumnImpl(n)
{
  xassert(mb);
  xassert(mb.size() == sizeof(T) * (n + 1));
  xassert(mb.get_element<T>(0) == 0);
  xassert(sb.size() == (mb.get_element<T>(n) & ~GETNA<T>()));
  _stype = stype_for<T>();
  mbuf = std::move(mb);
  strbuf = std::move(sb);
}





//==============================================================================
// Initialization methods
//==============================================================================

template <typename T>
void StringColumn<T>::init_data() {
  xassert(!ri);
  mbuf = MemoryRange::mem((_nrows + 1) * sizeof(T));
  mbuf.set_element<T>(0, 0);
}



//==============================================================================

template <typename T>
ColumnImpl* StringColumn<T>::shallowcopy() const {
  ColumnImpl* newcol = ColumnImpl::shallowcopy();
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
    T off1 = data_src[_nrows - 1] & ~GETNA<T>();
    new_strbuf_size = static_cast<size_t>(off1 - off0);
    if (!strbuf.is_writable()) {
      new_strbuf = MemoryRange::mem(new_strbuf_size);
      std::memcpy(new_strbuf.wptr(), strdata() + off0, new_strbuf_size);
    } else {
      std::memmove(new_strbuf.wptr(), strdata() + off0, new_strbuf_size);
    }
    for (size_t i = 0; i < _nrows; ++i) {
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
    for (size_t i = 0; i < _nrows; ++i, j += step) {
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
    //       be careful about cases where _nrows > T_MAX
  } else {
    const T* offs1 = offsets();
    const T* offs0 = offs1 - 1;
    T strs_size = 0;
    ri.iterate(0, _nrows, 1,
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
    ri.iterate(0, _nrows, 1,
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
    Column& thiscol, const RowIndex& replace_at, const Column& replace_with)
{
  materialize();
  Column rescol;

  Column with;
  if (replace_with) {
    with = replace_with;  // copy
    if (with.stype() != _stype) with = with.cast(_stype);
  }

  if (!with || with.nrows() == 1) {
    CString repl_value;  // Default constructor creates an NA string
    if (with) {
      bool isna = with.get_element(0, &repl_value);
      if (isna) repl_value = CString();
    }
    MemoryRange mask = replace_at.as_boolean_mask(_nrows);
    auto mask_indices = static_cast<const int8_t*>(mask.rptr());
    rescol = dt::map_str2str(thiscol,
      [=](size_t i, CString& value, dt::string_buf* sb) {
        sb->write(mask_indices[i]? repl_value : value);
      });
  }
  else {
    MemoryRange mask = replace_at.as_integer_mask(_nrows);
    auto mask_indices = static_cast<const int32_t*>(mask.rptr());
    rescol = dt::map_str2str(thiscol,
      [=](size_t i, CString& value, dt::string_buf* sb) {
        int ir = mask_indices[i];
        if (ir == -1) {
          sb->write(value);
        } else {
          CString str;
          bool isna = with.get_element(static_cast<size_t>(ir), &str);
          if (isna) {
            sb->write_na();
          } else {
            sb->write(str);
          }
        }
      });
  }

  xassert(rescol);
  if (rescol.stype() != _stype) {
    throw NotImplError() << "When replacing string values, the size of the "
      "resulting column exceeds the maximum for str32";
  }
  thiscol = std::move(rescol);
}


template <typename T>
void StringColumn<T>::resize_and_fill(size_t new_nrows)
{
  size_t old_nrows = _nrows;
  if (new_nrows == old_nrows) return;
  materialize();

  if (new_nrows > INT32_MAX && sizeof(T) == 4) {
    // TODO: instead of throwing an error, upcast the column to <uint64_t>
    // This is only an issue for the case when _nrows=1. Maybe we should separate
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
      set_value(offsets + _nrows, &na, sizeof(T), new_nrows - old_nrows);
    }
  }
  _nrows = new_nrows;
  // TODO: Temporary fix. To be resolved in #301
  if (stats != nullptr) stats->reset();
}



template <typename T>
void StringColumn<T>::apply_na_mask(const Column& mask) {
  xassert(mask.stype() == SType::BOOL);
  auto maskdata = static_cast<const int8_t*>(mask->data());
  char* strdata = static_cast<char*>(strbuf.wptr());
  T* offsets = this->offsets_w();

  // How much to reduce the offsets1 by due to some strings turning into NAs
  T doffset = 0;
  T offp = 0;
  for (size_t j = 0; j < _nrows; ++j) {
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
  size_t new_mbuf_size = sizeof(T) * (_nrows + 1);
  mbuf.resize(new_mbuf_size, /* keep_data = */ false);
  T* off_data = offsets_w();
  off_data[-1] = 0;

  dt::parallel_for_static(_nrows,
    [=](size_t i){
      off_data[i] = GETNA<T>();
    });
}




template <typename T>
void StringColumn<T>::fill_na_mask(int8_t* outmask, size_t row0, size_t row1) {
  const T* offs = this->offsets();
  for (size_t i = row0; i < row1; ++i) {
    outmask[i] = ISNA<T>(offs[i]);
  }
}




//------------------------------------------------------------------------------
// Explicit instantiation of the template
//------------------------------------------------------------------------------

template class StringColumn<uint32_t>;
template class StringColumn<uint64_t>;
