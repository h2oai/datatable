//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#include "column/sentinel_str.h"
#include "parallel/api.h"           // dt::parallel_for_static
#include "parallel/string_utils.h"  // dt::map_str2str
#include "python/string.h"
#include "utils/assert.h"
#include "utils/misc.h"
namespace dt {

template <typename T> constexpr SType stype_for() { return SType::VOID; }
template <> constexpr SType stype_for<uint32_t>() { return SType::STR32; }
template <> constexpr SType stype_for<uint64_t>() { return SType::STR64; }


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

// public: create a string column for `n` rows, preallocating the offsets array
// but leaving string buffer empty (and not allocated).
template <typename T>
SentinelStr_ColumnImpl<T>::SentinelStr_ColumnImpl(size_t n)
  : Sentinel_ColumnImpl(n, stype_for<T>())
{
  mbuf = Buffer::mem(sizeof(T) * (n + 1));
  static_cast<T*>(mbuf.wptr())[0] = 0;
}


// private use only
template <typename T>
SentinelStr_ColumnImpl<T>::SentinelStr_ColumnImpl()
  : Sentinel_ColumnImpl(0, stype_for<T>())  {}


// private: use `new_string_column(n, &&mb, &&sb)` instead
template <typename T>
SentinelStr_ColumnImpl<T>::SentinelStr_ColumnImpl(size_t n, Buffer&& mb, Buffer&& sb)
  : Sentinel_ColumnImpl(n, stype_for<T>())
{
  xassert(mb);
  xassert(mb.size() >= sizeof(T) * (n + 1));
  xassert(mb.get_element<T>(0) == 0);
  xassert(sb.size() >= (mb.get_element<T>(n) & ~GETNA<T>()));
  mbuf = std::move(mb);
  strbuf = std::move(sb);
}


template <typename T>
ColumnImpl* SentinelStr_ColumnImpl<T>::clone() const {
  return new SentinelStr_ColumnImpl<T>(nrows_, Buffer(mbuf), Buffer(strbuf));
}




//------------------------------------------------------------------------------
// Data buffers
//------------------------------------------------------------------------------

template <typename T>
size_t SentinelStr_ColumnImpl<T>::get_num_data_buffers() const noexcept {
  return 2;
}

template <typename T>
bool SentinelStr_ColumnImpl<T>::is_data_editable(size_t k) const {
  xassert(k <= 1);
  return false;
}

template <typename T>
size_t SentinelStr_ColumnImpl<T>::get_data_size(size_t k) const {
  xassert(k <= 1);
  if (k == 0) {
    xassert(mbuf.size() >= (nrows_ + 1) * sizeof(T));
    return (nrows_ + 1) * sizeof(T);
  }
  else {
    size_t sz = static_cast<const T*>(mbuf.rptr())[nrows_] & ~GETNA<T>();
    xassert(sz <= strbuf.size());
    return sz;
  }
}


template <typename T>
const void* SentinelStr_ColumnImpl<T>::get_data_readonly(size_t k) const {
  xassert(k <= 1);
  return (k == 0)? mbuf.rptr() : strbuf.rptr();
}


template <typename T>
void* SentinelStr_ColumnImpl<T>::get_data_editable(size_t k) {
  xassert(k <= 1);
  return (k == 0)? mbuf.xptr() : strbuf.xptr();
}


template <typename T>
Buffer SentinelStr_ColumnImpl<T>::get_data_buffer(size_t k) const {
  xassert(k <= 1);
  return (k == 0)? Buffer(mbuf) : Buffer(strbuf);
}




//------------------------------------------------------------------------------
// Data access
//------------------------------------------------------------------------------

template <typename T>
bool SentinelStr_ColumnImpl<T>::get_element(size_t i, CString* out) const {
  auto start_offsets = static_cast<const T*>(mbuf.rptr());
  T off_end = start_offsets[i + 1];
  if (ISNA<T>(off_end)) return false;
  T off_beg = start_offsets[i] & ~GETNA<T>();
  out->ch = static_cast<const char*>(strbuf.rptr()) + off_beg,
  out->size = static_cast<int64_t>(off_end - off_beg);
  return true;
}



//------------------------------------------------------------------------------
// Column operations
//------------------------------------------------------------------------------

template <typename T>
void SentinelStr_ColumnImpl<T>::replace_values(
    const RowIndex& replace_at, const Column& replace_with, Column& out)
{
  Column rescol;
  Column with;
  if (replace_with) {
    with = replace_with;  // copy
    if (with.stype() != stype_) with = with.cast(stype_);
  }

  if (!with || with.nrows() == 1) {
    CString repl_value;  // Default constructor creates an NA string
    if (with) {
      bool isvalid = with.get_element(0, &repl_value);
      if (!isvalid) repl_value = CString();
    }
    Buffer mask = replace_at.as_boolean_mask(nrows_);
    auto mask_indices = static_cast<const int8_t*>(mask.rptr());
    rescol = dt::map_str2str(out,
      [=](size_t i, CString& value, dt::string_buf* sb) {
        sb->write(mask_indices[i]? repl_value : value);
      });
  }
  else {
    Buffer mask = replace_at.as_integer_mask(nrows_);
    auto mask_indices = static_cast<const int32_t*>(mask.rptr());
    rescol = dt::map_str2str(out,
      [=](size_t i, CString& value, dt::string_buf* sb) {
        int ir = mask_indices[i];
        if (ir == -1) {
          sb->write(value);
        } else {
          CString str;
          bool isvalid = with.get_element(static_cast<size_t>(ir), &str);
          if (isvalid) {
            sb->write(str);
          } else {
            sb->write_na();
          }
        }
      });
  }

  xassert(rescol);
  if (rescol.stype() != stype_) {
    throw NotImplError() << "When replacing string values, the size of the "
      "resulting column exceeds the maximum for str32";
  }
  out = std::move(rescol);
}




template class SentinelStr_ColumnImpl<uint32_t>;
template class SentinelStr_ColumnImpl<uint64_t>;

}  // namespace dt
