//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//------------------------------------------------------------------------------
#include <cstring>        // std::memcpy
#include "column.h"
#include "datatablemodule.h"
#include "wstringcol.h"

namespace dt {


//------------------------------------------------------------------------------
// writable_string_col
//------------------------------------------------------------------------------

writable_string_col::writable_string_col(size_t nrows, bool str64_)
  : strdata(nrows),
    offdata(Buffer::mem((nrows + 1) * 4 * (1 + str64_))),
    n(nrows),
    str64(str64_) {}

writable_string_col::writable_string_col(Buffer&& offsets, size_t nrows,
                                         bool str64_)
  : strdata(nrows),
    offdata(std::move(offsets)),
    n(nrows),
    str64(str64_)
{
  offdata.resize((nrows + 1) * 4 * (1 + str64));
}


Column writable_string_col::to_ocolumn() && {
  strdata.finalize();
  auto strbuf = strdata.get_mbuf();
  if (str64) {
    offdata.set_element<uint64_t>(0, 0);
  } else {
    offdata.set_element<uint32_t>(0, 0);
  }
  return Column::new_string_column(n, std::move(offdata), std::move(strbuf));
}


using pbuf = std::unique_ptr<writable_string_col::buffer>;
pbuf writable_string_col::make_buffer() {
  return str64? pbuf(new writable_string_col::buffer_impl<uint64_t>(*this))
              : pbuf(new writable_string_col::buffer_impl<uint32_t>(*this));
}



//------------------------------------------------------------------------------
// writable_string_col::buffer
//------------------------------------------------------------------------------

writable_string_col::buffer::buffer() {
  TRACK(this, sizeof(*this), "writable_string_col::buffer");
}

writable_string_col::buffer::~buffer() {
  UNTRACK(this);
}


void writable_string_col::buffer::write(const CString& str) {
  write(str.ch, static_cast<size_t>(str.size));
}


void writable_string_col::buffer::write(const std::string& str) {
  write(str.data(), str.size());
}


void writable_string_col::buffer::write_na() {
  write(nullptr, 0);
}




//------------------------------------------------------------------------------
// writable_string_col::buffer_impl
//------------------------------------------------------------------------------

template <typename T>
writable_string_col::buffer_impl<T>::buffer_impl(writable_string_col& s)
  : col(s),
    strbuf(1024),
    strbuf_used(0),
    strbuf_write_pos(0),
    offptr(nullptr),
    offptr0(nullptr) {}


template <typename T>
void writable_string_col::buffer_impl<T>::write(const char* ch, size_t len) {
  if (ch) {
    if (sizeof(T) == 4) xassert(len <= Column::MAX_ARR32_SIZE);
    strbuf.ensuresize(strbuf_used + len);
    std::memcpy(strbuf.data() + strbuf_used, ch, len);
    strbuf_used += len;
    *offptr++ = static_cast<T>(strbuf_used);
  } else {
    // Use XOR instead of OR in case the buffer overflows uint32_t.
    *offptr++ = static_cast<T>(strbuf_used) ^ GETNA<T>();
  }
}


template <typename T>
void writable_string_col::buffer_impl<T>::order() {
  strbuf_write_pos = col.strdata.prep_write(strbuf_used, strbuf.data());
}


template <typename T>
void writable_string_col::buffer_impl<T>::commit_and_start_new_chunk(size_t i0)
{
  col.strdata.write_at(strbuf_write_pos, strbuf_used, strbuf.data());
  for (T* p = offptr0; p < offptr; ++p) {
    *p += strbuf_write_pos;
  }
  offptr = static_cast<T*>(col.offdata.xptr()) + i0 + 1;
  offptr0 = offptr;
  strbuf_used = 0;
}


template <typename T>
char* writable_string_col::buffer_impl<T>::prepare_raw_write(size_t nbytes) {
  strbuf.ensuresize(strbuf_used + nbytes);
  return strbuf.data() + strbuf_used;
}


template <typename T>
void writable_string_col::buffer_impl<T>::commit_raw_write(char* ptr) {
  strbuf_used = static_cast<size_t>(ptr - strbuf.data());
  *offptr++ = static_cast<T>(strbuf_used);
}



template class writable_string_col::buffer_impl<uint32_t>;
template class writable_string_col::buffer_impl<uint64_t>;

} // namespace dt
