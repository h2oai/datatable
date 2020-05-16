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
#ifndef dt_READ_OUTPUT_COLUMN_h
#define dt_READ_OUTPUT_COLUMN_h
#include "_dt.h"
#include "buffer.h"
namespace dt {
namespace read {


/**
  */
class OutputColumn
{
  friend class InputColumn;
  private:
    Buffer databuf_;
    std::unique_ptr<MemoryWritableBuffer> strbuf_;
    std::vector<Column> chunks_;
    size_t nrows_in_chunks_;
    size_t na_count_;
    SType stype_;

    bool type_bumped_;        // tmp
    bool present_in_buffer_;  // tmp
    size_t : 40;

  public:
    OutputColumn();
    OutputColumn(OutputColumn&&) noexcept;
    OutputColumn(const OutputColumn&) = delete;

    void* data_w();
    WritableBuffer* strdata_w();
    void allocate(size_t new_nrows);
    void archive_data(size_t nrows_written, std::shared_ptr<TemporaryFile>&);
    Column to_column();

    void set_stype(SType stype);
};




}} // namespace dt::read
#endif
