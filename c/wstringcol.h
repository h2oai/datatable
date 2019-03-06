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
    bool str64;
    size_t : 56;

  public:
    writable_string_col(size_t nrows, bool str64_ = false);
    writable_string_col(MemoryRange&& offsets, size_t nrows,
                        bool str64_ = false);
    Column* to_column() &&;

    class buffer {
      public:
        buffer();
        virtual ~buffer();
        virtual void write(const char* ch, size_t len) = 0;
        void write(const std::string&);
        void write(const CString&);
        void write_na();
        virtual char* prepare_raw_write(size_t nbytes) = 0;
        virtual void commit_raw_write(char* ptr) = 0;

        virtual void order() = 0;
        virtual void commit_and_start_new_chunk(size_t i0) = 0;
    };

    template <typename T>
    class buffer_impl : public buffer {
      private:
        writable_string_col& col;
        dt::array<char> strbuf;
        size_t strbuf_used;
        size_t strbuf_write_pos;
        T* offptr;
        T* offptr0;

      public:
        buffer_impl(writable_string_col&);
        using buffer::write;
        void write(const char* ch, size_t len) override;
        char* prepare_raw_write(size_t nbytes) override;
        void commit_raw_write(char* ptr) override;

        void order() override;
        void commit_and_start_new_chunk(size_t i0) override;
    };
};



extern template class writable_string_col::buffer_impl<uint32_t>;
extern template class writable_string_col::buffer_impl<uint64_t>;


}
#endif
