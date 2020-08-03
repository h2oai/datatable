//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#include "column/sentinel_str.h"
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
  offbuf_ = Buffer::mem(sizeof(T) * (n + 1));
  static_cast<T*>(offbuf_.wptr())[0] = 0;
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
  // xassert(mb.get_element<T>(0) == 0);
  // xassert(sb.size() >= (mb.get_element<T>(n) & ~GETNA<T>()));
  offbuf_ = std::move(mb);
  strbuf_ = std::move(sb);
}


template <typename T>
ColumnImpl* SentinelStr_ColumnImpl<T>::clone() const {
  return new SentinelStr_ColumnImpl<T>(nrows_, Buffer(offbuf_), Buffer(strbuf_));
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
  (void) k;
  return false;
}

template <typename T>
size_t SentinelStr_ColumnImpl<T>::get_data_size(size_t k) const {
  xassert(k <= 1);
  if (k == 0) {
    // xassert(offbuf_.size() >= (nrows_ + 1) * sizeof(T));
    // return (nrows_ + 1) * sizeof(T);
    return offbuf_.size();
  }
  else {
    // size_t sz = static_cast<const T*>(offbuf_.rptr())[nrows_] & ~GETNA<T>();
    // xassert(sz <= strbuf_.size());
    // return sz;
    return strbuf_.size();
  }
}


template <typename T>
const void* SentinelStr_ColumnImpl<T>::get_data_readonly(size_t k) const {
  xassert(k <= 1);
  return (k == 0)? offbuf_.rptr() : strbuf_.rptr();
}


template <typename T>
void* SentinelStr_ColumnImpl<T>::get_data_editable(size_t k) {
  xassert(k <= 1);
  return (k == 0)? offbuf_.wptr() : strbuf_.wptr();
}


template <typename T>
Buffer SentinelStr_ColumnImpl<T>::get_data_buffer(size_t k) const {
  xassert(k <= 1);
  return (k == 0)? Buffer(offbuf_) : Buffer(strbuf_);
}


template <typename T>
void SentinelStr_ColumnImpl<T>::materialize(Column&, bool to_memory) {
  if (to_memory) {
    offbuf_.to_memory();
    strbuf_.to_memory();
  }
}




//------------------------------------------------------------------------------
// Data access
//------------------------------------------------------------------------------

template <typename T>
bool SentinelStr_ColumnImpl<T>::get_element(size_t i, CString* out) const {
  auto start_offsets = static_cast<const T*>(offbuf_.rptr());
  T off_end = start_offsets[i + 1];
  if (ISNA<T>(off_end)) return false;
  T off_beg = start_offsets[i] & ~GETNA<T>();
  *out = CString(static_cast<const char*>(strbuf_.rptr()) + off_beg,
                 static_cast<size_t>(off_end - off_beg));
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
      [&](size_t i, CString& value, dt::string_buf* sb) {
        sb->write(mask_indices[i]? repl_value : value);
      });
  }
  else {
    Buffer mask = replace_at.as_integer_mask(nrows_);
    auto mask_indices = static_cast<const int32_t*>(mask.rptr());
    rescol = dt::map_str2str(out,
      [=](size_t i, CString& value, dt::string_buf* sb) {
        int ir = mask_indices[i];
        if (ir == RowIndex::NA<int32_t>) {
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
  // Note: it's possible that rescol.stype() != this->stype()
  out = std::move(rescol);
}




template class SentinelStr_ColumnImpl<uint32_t>;
template class SentinelStr_ColumnImpl<uint64_t>;

}  // namespace dt
