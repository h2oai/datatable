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
#ifndef dt_WSTRINGCOL_h
#define dt_WSTRINGCOL_h
#include <memory>           // std::unique_ptr
#include "memrange.h"       // MemoryRange
#include "types.h"          // CString
#include "utils/array.h"    // dt::array
#include "writebuf.h"       // WritableBuffer

class Column;
template <typename T> class StringColumn;

namespace dt {

// TODO: merge into parallel.h

//------------------------------------------------------------------------------
// (Fixed-height) writable string column
//------------------------------------------------------------------------------

/**
 * This class can be used to create a string column of known height (i.e. its
 * number of rows is known in advance).
 *
 */
class writable_string_col {
  private:
    MemoryWritableBuffer strdata;
    MemoryRange offdata;
    size_t n;

    static constexpr size_t MAX_ITEM_SIZE = 0x7FFFFFFF;
    static constexpr size_t MAX_STR32_BUFFER_SIZE = 0x7FFFFFFF;
    static constexpr size_t MAX_STR32_NROWS = 0x7FFFFFFF;

  public:
    writable_string_col(size_t nrows);
    Column* to_column() &&;

    class buffer {
      private:
        writable_string_col& col;
        dt::array<char> strbuf;
        size_t strbuf_used;
        size_t strbuf_write_pos;
        uint32_t* offptr;
        uint32_t* offptr0;

      public:
        buffer(writable_string_col&);

        void write(const CString&);
        void write(const std::string&);
        void write(const char* ch, size_t len);
        void write_na();

        void order();
        void commit_and_start_new_chunk(size_t i0);
    };
};


using fixed_height_string_col = writable_string_col;



}
#endif
