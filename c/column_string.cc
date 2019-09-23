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
StringColumn<T>::StringColumn(size_t n)
  : ColumnImpl(n, stype_for<T>())
{
  mbuf = MemoryRange::mem(sizeof(T) * (n + 1));
  mbuf.set_element<T>(0, 0);
}


// private use only
template <typename T>
StringColumn<T>::StringColumn()
  : ColumnImpl(0, stype_for<T>())  {}


// private: use `new_string_column(n, &&mb, &&sb)` instead
template <typename T>
StringColumn<T>::StringColumn(size_t n, MemoryRange&& mb, MemoryRange&& sb)
  : ColumnImpl(n, stype_for<T>())
{
  xassert(mb);
  xassert(mb.size() == sizeof(T) * (n + 1));
  xassert(mb.get_element<T>(0) == 0);
  xassert(sb.size() == (mb.get_element<T>(n) & ~GETNA<T>()));
  mbuf = std::move(mb);
  strbuf = std::move(sb);
}





//==============================================================================
// Initialization methods
//==============================================================================

template <typename T>
void StringColumn<T>::init_data() {
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
  const T* offs = this->offsets();
  T off_end = offs[i];
  if (ISNA<T>(off_end)) return true;
  T off_beg = offs[i - 1] & ~GETNA<T>();
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
ColumnImpl* StringColumn<T>::materialize() {
  return this;
}


template <typename T>
void StringColumn<T>::replace_values(
    Column& thiscol, const RowIndex& replace_at, const Column& replace_with)
{
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





//------------------------------------------------------------------------------
// Explicit instantiation of the template
//------------------------------------------------------------------------------

template class StringColumn<uint32_t>;
template class StringColumn<uint64_t>;
