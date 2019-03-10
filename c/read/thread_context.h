//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_READ_THREADCONTEXT_h
#define dt_READ_THREADCONTEXT_h
#include "read/field64.h"    // field64
#include "read/chunk_coordinates.h"  // ChunkCoordinates
#include "utils/array.h"     // dt::array
namespace dt {
namespace read {


/**
 * This is a helper class for ChunkedReader. It carries variables that will be
 * local to each thread during the parallel reading of the input.
 *
 * tbuf
 *   Output buffer. Within the buffer the data is stored in row-major order,
 *   i.e. in the same order as in the original CSV file. We view the buffer as
 *   a rectangular grid having `tbuf_ncols * tbuf_nrows` elements (+1 extra).
 *
 * sbuf
 *   Additional buffer, used to store string content after post-processing.
 *
 * tbuf_ncols, tbuf_nrows
 *   Dimensions of the output buffer.
 *
 * used_nrows
 *   Number of rows of data currently stored in `tbuf`. This can never exceed
 *   `tbuf_nrows`.
 *
 * row0
 *   Starting row index within the output DataTable for the current data chunk.
 */
class ThreadContext {
  public:
    struct SInfo { size_t start, size, write_at; };

    dt::array<field64> tbuf;
    dt::array<uint8_t> sbuf;
    dt::array<SInfo> strinfo;
    size_t tbuf_ncols;
    size_t tbuf_nrows;
    size_t used_nrows;
    size_t row0;

  public:
    ThreadContext(size_t ncols, size_t nrows);
    virtual ~ThreadContext();

    virtual void push_buffers() = 0;
    virtual void read_chunk(const ChunkCoordinates&, ChunkCoordinates&) = 0;
    virtual void orderBuffer() = 0;

    size_t get_nrows() const;
    void set_nrows(size_t n);
    void allocate_tbuf(size_t ncols, size_t nrows);
};


}  // namespace read
}  // namespace dt

#endif
