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
namespace dt {
namespace read {


/**
  * This is a helper class for ParallelReader. It carries variables
  * that will be local to each thread during the parallel reading
  * of the input.
  *
  * This class is abstract. The derived classes are expected to
  * implement the actual logic for reading a chunk into the local
  * buffers (tbuf/sbuf) and then saving into the output PreFrame.
  *
  * tbuf
  *   Output buffer. Within the buffer the data is stored in row-major
  *   order, i.e. in the same order as in the original CSV file. We
  *   view the buffer as a rectangular grid having `tbuf_ncols *
  *   tbuf_nrows` elements (+1 extra).
  *
  * sbuf
  *   Additional buffer, used to store string content after post-
  *   processing.
  *
  * strinfo
  *   Information necessary for string columns. The number of elements
  *   in this vector should be equal to the number of string columns
  *   in the preframe.
  *   Each entry is a struct:
  *     .start    - location of this column's character data within
  *                 the `sbuf` buffer;
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
    struct StrInfo {
      size_t start;
      size_t size;
      size_t write_at;
    };

    std::vector<field64> tbuf;
    std::vector<uint8_t> sbuf;
    std::vector<StrInfo> strinfo;
    size_t tbuf_ncols;
    size_t tbuf_nrows;
    size_t used_nrows;
    size_t row0;

    PreFrame& preframe_;

  public:
    ThreadContext(size_t ncols, size_t nrows, PreFrame&);
    virtual ~ThreadContext();

    virtual void read_chunk(const ChunkCoordinates&, ChunkCoordinates&) = 0;
    virtual void order_buffer();
    virtual void push_buffers() = 0;

    size_t get_nrows() const;
    void set_nrows(size_t n);
    void set_row0(size_t n);
    void allocate_tbuf(size_t ncols, size_t nrows);
};




}}  // namespace dt::read
#endif
