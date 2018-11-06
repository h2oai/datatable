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
// fixed_height_string_col
//------------------------------------------------------------------------------

fixed_height_string_col::fixed_height_string_col(size_t nrows)
  : strdata(new MemoryWritableBuffer(nrows)),
    offdata(MemoryRange::mem((nrows + 1) * sizeof(uint32_t))),
    n(nrows) {}


Column* fixed_height_string_col::to_column() && {
  strdata->finalize();
  offdata.set_element<uint32_t>(0, 0);
  return new StringColumn<uint32_t>(n, std::move(offdata), strdata->get_mbuf());
}


fixed_height_string_col::buffer::buffer(fixed_height_string_col& s)
  : col(s), strbuf(1024)
{
  strbuf_used = 0;
  strbuf_write_pos = 0;
  offptr = nullptr;
  offptr0 = nullptr;
}


void fixed_height_string_col::buffer::write(const CString& str) {
  write(str.ch, static_cast<size_t>(str.size));
}

void fixed_height_string_col::buffer::write(const std::string& str) {
  write(str.data(), str.size());
}

void fixed_height_string_col::buffer::write(const char* ch, size_t len) {
  if (ch) {
    strbuf.ensuresize(strbuf_used + len);
    std::memcpy(strbuf.data() + strbuf_used, ch, len);
    strbuf_used += len;
    *offptr++ = static_cast<uint32_t>(strbuf_used);
  } else {
    *offptr++ = static_cast<uint32_t>(strbuf_used) | GETNA<uint32_t>();
  }
}

void fixed_height_string_col::buffer::write_na() {
  *offptr++ = static_cast<uint32_t>(strbuf_used) | GETNA<uint32_t>();
}


void fixed_height_string_col::buffer::order() {
  strbuf_write_pos = col.strdata->prep_write(strbuf_used, strbuf.data());
}


void fixed_height_string_col::buffer::commit_and_start_new_chunk(size_t i0) {
  col.strdata->write_at(strbuf_write_pos, strbuf_used, strbuf.data());
  for (uint32_t* p = offptr0; p < offptr; ++p) {
    *p += strbuf_write_pos;
  }
  offptr = static_cast<uint32_t*>(col.offdata.xptr()) + i0 + 1;
  offptr0 = offptr;
  strbuf_used = 0;
}


} // namespace dt
