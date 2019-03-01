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


Column* writable_string_col::to_column() && {
  strdata.finalize();
  auto strbuf = strdata.get_mbuf();
  offdata.set_element<uint32_t>(0, 0);
  return new_string_column(n, std::move(offdata), std::move(strbuf));
  /*
  if (strbuf.size() <= MAX_STR32_BUFFER_SIZE && n <= MAX_STR32_NROWS) {
    return new StringColumn<uint32_t>(n, std::move(offdata),
                                      std::move(strbuf));
  }
  else {
    // Offsets need to be recoded into uint64_t array
    // TODO: this conversion can also be done in parallel, using
    //
    MemoryRange offsets64 = MemoryRange::mem(sizeof(uint64_t) * (n + 1));
    auto data64 = static_cast<uint64_t*>(offsets64.xptr());
    auto data32 = static_cast<const uint32_t*>(offdata.rptr());
    uint64_t curr_offset = 0;
    data64[0] = 0;
    for (size_t i = 1; i <= n; ++i) {
      uint32_t len = data32[i] - data32[i - 1];
      if (len == GETNA<uint32_t>()) {
        data64[i] = curr_offset | GETNA<uint64_t>();
      } else {
        curr_offset += len;
        data64[i] = curr_offset;
      }
    }
    xassert(curr_offset == strbuf.size());
    return new StringColumn<uint64_t>(n, std::move(offsets64),
                                      std::move(strbuf));
  }
  */
}



//------------------------------------------------------------------------------
// writable_string_col::buffer
//------------------------------------------------------------------------------

writable_string_col::buffer::buffer(writable_string_col& s)
  : col(s),
    strbuf(1024),
    strbuf_used(0),
    strbuf_write_pos(0),
    offptr(nullptr),
    offptr0(nullptr) {}


void writable_string_col::buffer::write(const CString& str) {
  write(str.ch, static_cast<size_t>(str.size));
}

void writable_string_col::buffer::write(const std::string& str) {
  write(str.data(), str.size());
}

void writable_string_col::buffer::write(const char* ch, size_t len) {
  // Note: `strbuf_used` may overflow during the conversion into uint32_t.
  // If this happens, we would have to convert into STR64. It will be
  // possible to restore the correct offsets, provided that no single string
  // item exceeds MAX_ITEM_SIZE in size.
  if (ch) {
    xassert(len <= MAX_ITEM_SIZE);
    strbuf.ensuresize(strbuf_used + len);
    std::memcpy(strbuf.data() + strbuf_used, ch, len);
    strbuf_used += len;
    *offptr++ = static_cast<uint32_t>(strbuf_used);
  } else {
    // Use XOR instead of OR in case the buffer overflows int32_t.
    *offptr++ = static_cast<uint32_t>(strbuf_used) ^ GETNA<uint32_t>();
  }
}

void writable_string_col::buffer::write_na() {
  *offptr++ = static_cast<uint32_t>(strbuf_used) ^ GETNA<uint32_t>();
}


void writable_string_col::buffer::order() {
  strbuf_write_pos = col.strdata.prep_write(strbuf_used, strbuf.data());
}


void writable_string_col::buffer::commit_and_start_new_chunk(size_t i0) {
  col.strdata.write_at(strbuf_write_pos, strbuf_used, strbuf.data());
  for (uint32_t* p = offptr0; p < offptr; ++p) {
    *p += strbuf_write_pos;
  }
  offptr = static_cast<uint32_t*>(col.offdata.xptr()) + i0 + 1;
  offptr0 = offptr;
  strbuf_used = 0;
}


} // namespace dt
