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
#include "wstringcol.h"
#include <cstring>        // std::memcpy
#include "column.h"

namespace dt {


//------------------------------------------------------------------------------
// writable_string_col
//------------------------------------------------------------------------------

writable_string_col::writable_string_col(size_t nrows)
  : strdata(nrows),
    offdata(MemoryRange::mem((nrows + 1) * sizeof(uint32_t))),
    n(nrows) {}

writable_string_col::writable_string_col(MemoryRange&& offsets, size_t nrows)
  : strdata(nrows),
    offdata(std::move(offsets)),
    n(nrows)
{
  offdata.resize((nrows + 1) * sizeof(uint32_t));
}


Column* writable_string_col::to_column() && {
  strdata.finalize();
  auto strbuf = strdata.get_mbuf();
  offdata.set_element<uint32_t>(0, 0);
  return new_string_column(n, std::move(offdata), std::move(strbuf));
}



//------------------------------------------------------------------------------
// writable_string_col::buffer
//------------------------------------------------------------------------------

writable_string_col::buffer::~buffer() {}


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
    xassert(sizeof(T) == 4? len <= Column::MAX_STRING_SIZE : true);
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




template class writable_string_col::buffer_impl<uint32_t>;
template class writable_string_col::buffer_impl<uint64_t>;

} // namespace dt
