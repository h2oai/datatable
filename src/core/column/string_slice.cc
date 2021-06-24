//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#include <cstring>
#include "column/string_slice.h"
namespace dt {


StringSlice_ColumnImpl::StringSlice_ColumnImpl(
    Column&& src, Column&& start, Column&& stop, Column&& step)
  : Virtual_ColumnImpl(src.nrows(), src.stype()),
    src_(std::move(src)),
    start_(std::move(start)),
    stop_(std::move(stop)),
    step_(std::move(step))
{
  xassert(src_.nrows() == nrows_ && src_.can_be_read_as<CString>());
  xassert(start_.nrows() == nrows_ && start_.can_be_read_as<int64_t>());
  xassert(stop_.nrows() == nrows_ && stop_.can_be_read_as<int64_t>());
  xassert(step_.nrows() == nrows_ && step_.can_be_read_as<int64_t>());
}


ColumnImpl* StringSlice_ColumnImpl::clone() const {
  return new StringSlice_ColumnImpl(
      Column(src_), Column(start_), Column(stop_), Column(step_)
  );
}


size_t StringSlice_ColumnImpl::n_children() const noexcept {
  return 4;
}

const Column& StringSlice_ColumnImpl::child(size_t i) const {
  xassert(i <= 3);
  return i == 0? src_ : i == 1? start_ : i == 2? stop_ : step_;
}


/**
  * Iterate over utf8 string `[*pch, end)` and move the pointer `*pch`
  * to a character at position `index` in the string. If the `index`
  * is larger than the length of the string, then move to `end`
  * instead. If the `index` is negative, then the pointer is not moved.
  */
static void moveToIndex(int64_t index, const uint8_t** pch, const uint8_t* end) {
  auto ch = *pch;
  int64_t i = 0;
  while (ch < end && i < index) {
    uint8_t c = *ch;
    ch += (c < 0x80)? 1 :
          ((c & 0xE0) == 0xC0)? 2 :
          ((c & 0xF0) == 0xE0)? 3 : 4;
    i++;
  }
  *pch = ch;
}

/**
  * Calculate and return unicode length of string `[ch, end)`.
  */
static int64_t getStringLength(const uint8_t* ch, const uint8_t* end) {
  int64_t i = 0;
  while (ch < end) {
    uint8_t c = *ch;
    ch += (c < 0x80)? 1 :
          ((c & 0xE0) == 0xC0)? 2 :
          ((c & 0xF0) == 0xE0)? 3 : 4;
    i++;
  }
  return i;
}


bool StringSlice_ColumnImpl::get_element(size_t i, CString* out) const {
  CString str;
  bool isvalid = src_.get_element(i, &str);
  if (!isvalid) return false;
  if (!str.size()) {  // Any slice of an empty string is still empty
    *out = std::move(str);
    return true;
  }
  auto ch = reinterpret_cast<const uint8_t*>(str.data());
  auto eof = ch + str.size();

  int64_t istart, istop, istep;
  bool startValid = start_.get_element(i, &istart);
  bool stopValid = stop_.get_element(i, &istop);
  bool stepValid = step_.get_element(i, &istep);

  if (!stepValid || istep == 1) {
    int64_t ilength = 0;  // Number of unicode characters in `str`
    if (!startValid) istart = 0;
    if (istart < 0) {
      ilength = getStringLength(ch, eof);
      istart += ilength;
      if (istart < 0) istart = 0;
    }
    xassert(istart >= 0);
    moveToIndex(istart, &ch, eof);
    xassert(ch <= eof);

    if (!stopValid) {
      auto len = static_cast<size_t>(eof - ch);
      char* ptr = out->prepare_buffer(len);
      if (len) std::memcpy(ptr, ch, len);
      return true;
    }
    if (istop < 0) {
      if (ilength == 0) ilength = getStringLength(ch, eof) + istart;
      istop += ilength;
    }
    auto ch0 = ch;
    moveToIndex(istop - istart, &ch, eof);  // noop if istop<=istart
    auto len = static_cast<size_t>(ch - ch0);
    char* ptr = out->prepare_buffer(len);
    if (len) std::memcpy(ptr, ch0, len);
    return true;
  }
  else if (istep > 1) {
    throw NotImplError() << "Cannot create string slice with step > 1";
  }
  else if (istep < 0) {
    throw NotImplError() << "Cannot create string slice with step < 0";
  }
  else {
    xassert(istep == 0);
    return false;  // Step of 0 is invalid in a slice
  }
}




}  // namespace dt
