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
#include "read/colinfo.h"
namespace dt {
namespace read {


/**
  * This class represents a single column that will be written
  * to the output frame.
  *
  * The column contains the main data buffer `databuf_`, plus an
  * extra buffer for string data `strbuf_`. Additionally, there is
  * vector `chunks_` which contains data that was already stored as
  * `Column` fragments.
  *
  * TODO: finish separating from InputColumn
  */
class OutputColumn
{
  friend class InputColumn;
  private:
    Buffer databuf_;
    std::unique_ptr<MemoryWritableBuffer> strbuf_;
    std::vector<Column> chunks_;
    size_t nrows_in_chunks_;
    size_t nrows_allocated_;
    ColInfo colinfo_;
    SType stype_;

    size_t : 56;

  public:
    OutputColumn();
    OutputColumn(OutputColumn&&) noexcept = default;
    OutputColumn(const OutputColumn&) = delete;

    // Return pointer to the main data buffer for writing at the
    // given row number.
    void* data_w(size_t row) const;

    // Return the MemoryWritableBuffer object for writing string data.
    //
    MemoryWritableBuffer* strdata_w();

    // Resize `databuf_` / `strbuf_` so that the total number of rows
    // that can be written to this OutputColumn was `nrows`. This
    // total number of rows includes the rows that were already
    // archived. The argument `nrows` may be smaller than the previous
    // requested allocation (which will cause the data buffers to
    // shrink), but must be greater than the number of archived rows.
    //
    void allocate(size_t nrows);

    // Convert the current data, as stored in databuf_/strbuf_ into
    // a Column, and store that column in the `chunks_` vector. This
    // method must be called before calling `to_column()`, or before
    // changing the OutputColumn's stype.
    //
    // The `nrows_written` argument tells how many rows of data were
    // already written into the output buffer. This value should
    // include those rows that were previously archived.
    //
    // This method may only be called under the circumstances when
    // no other thread accesses this class or writes into its buffers.
    // After calling this method, all prior written rows can no longer
    // be accessed.
    //
    void archive_data(size_t nrows_written, std::shared_ptr<TemporaryFile>&);

    // Convert this object into a finalized `Column`. Call
    // `archive_data()` before calling this method. The returned
    // column will be either a regular "material" column if there is
    // only one item in the `chunks_` vector, or an "rbound" column
    // if there are multiple chunks.
    //
    Column to_column();

    void merge_chunk_stats(const ColInfo& info);

    void set_stype(SType stype);
    void set_stype(SType stype, size_t nrows_written,
                   std::shared_ptr<TemporaryFile>& tempfile);

  private:
    void reset_colinfo();
};




}} // namespace dt::read
#endif
