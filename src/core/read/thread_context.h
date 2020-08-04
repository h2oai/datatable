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
#ifndef dt_READ_THREADCONTEXT_h
#define dt_READ_THREADCONTEXT_h
#include "_dt.h"
#include "parallel/api.h"
#include "read/colinfo.h"
#include "read/parse_context.h"
namespace dt {
namespace read {


/**
  * This is a helper class for ParallelReader. It carries variables
  * that will be local to each thread during the parallel reading
  * of the input.
  *
  * This class is abstract. The derived classes are expected to
  * implement the actual logic for reading a chunk into the local
  * buffers (tbuf) and then saving into the output PreFrame.
  *
  * tbuf
  *   Output buffer. Within the buffer the data is stored in row-major
  *   order, i.e. in the same order as in the original CSV file. We
  *   view the buffer as a rectangular grid having `tbuf_ncols *
  *   tbuf_nrows` elements (+1 extra).
  *
  * strinfo
  *   Information necessary for string columns. The number of elements
  *   in this vector should be equal to the number of string columns
  *   in the preframe.
  *   Each entry is a struct:
  *     .size     - size of the character data buffer;
  *     .write_at - where the data should be written within the
  *                 column's WritableBuffer.
  *
  * tbuf_ncols, tbuf_nrows
  *   Dimensions of the output buffer.
  *
  * used_nrows
  *   Number of rows of data currently stored in `tbuf`. This can
  *   never exceed `tbuf_nrows`.
  *
  * row0
  *   Starting row index within the PreFrame for the current data
  *   chunk.
  */
class ThreadContext    // TODO: rename
{
  protected:
    std::vector<field64> tbuf;
    std::vector<ColInfo> colinfo_;
    size_t tbuf_ncols;
    size_t tbuf_nrows;
    size_t used_nrows;
    size_t row0_;

    PreFrame& preframe_;
    ParseContext parse_ctx_;

  public:
    ThreadContext(size_t ncols, size_t nrows, PreFrame&);
    virtual ~ThreadContext();

    virtual void read_chunk(const ChunkCoordinates&, ChunkCoordinates&) = 0;
    virtual void preorder();
    virtual void order();
    virtual void postorder();
    virtual bool handle_typebumps(OrderedTask*) { return false; }

    size_t get_nrows() const;
    void set_nrows(size_t n);
    void set_row0();
    void allocate_tbuf(size_t ncols, size_t nrows);

    size_t ensure_output_nrows(size_t chunk_nrows, size_t ichunk, dt::OrderedTask*);

  private:
    void preorder_bool_column(size_t j);
    void preorder_int32_column(size_t j);
    void preorder_int64_column(size_t j);
    void preorder_float32_column(size_t j);
    void preorder_float64_column(size_t j);
    void preorder_string_column(size_t j);

    void postorder_bool_column(OutputColumn& col, size_t j);
    void postorder_int32_column(OutputColumn& col, size_t j);
    void postorder_int64_column(OutputColumn& col, size_t j);
    void postorder_float32_column(OutputColumn& col, size_t j);
    void postorder_float64_column(OutputColumn& col, size_t j);
    void postorder_string_column(OutputColumn& col, size_t j);
};




}}  // namespace dt::read
#endif
