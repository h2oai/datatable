//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_READ_PARALLELREADER_h
#define dt_READ_PARALLELREADER_h
#include <memory>                    // std::unique_ptr
#include "read/chunk_coordinates.h"  // ChunkCoordinates
#include "read/thread_context.h"     // ThreadContext

class GenericReader;

namespace dt {
namespace read {
using ThreadContextPtr = std::unique_ptr<ThreadContext>;



/**
 * This class' responsibility is to execute parallel reading of its input,
 * ensuring that the data integrity is maintained.
 */
class ParallelReader {

  protected:
    size_t chunk_size;
    size_t chunk_count;
    const char* input_start;
    const char* input_end;
    const char* end_of_last_chunk;
    double approximate_line_length;

  protected:
    GenericReader& g;
    shared_mutex shmutex;
    size_t nrows_max;
    size_t nrows_allocated;
    size_t nrows_written;
    size_t nthreads;

  public:
    ParallelReader(GenericReader& reader, double len);
    ParallelReader(const ParallelReader&) = delete;
    ParallelReader(ParallelReader&&) = delete;
    ParallelReader& operator=(const ParallelReader&) = delete;
    virtual ~ParallelReader();

    virtual void read_all();

  protected:
    /**
     * This method can be overridden in derived classes in order to implement
     * more advanced chunk boundaries detection. This method will be called from
     * within `compute_chunk_boundaries()` only.
     * This method should modify `cc` by-reference; making sure not to alter
     * `start` / `end` if the flags `start_exact` / `end_exact` are set.
     */
    virtual void adjust_chunk_coordinates(
        ChunkCoordinates&, ThreadContextPtr&) const {}

    /**
     * Return an instance of a `ThreadContext` class. Implementations of
     * `ParallelReader` are expected to override this method to return
     * appropriate subclasses of `ThreadContext`.
     */
    virtual ThreadContextPtr init_thread_context() = 0;

  private:
    ChunkCoordinates compute_chunk_boundaries(size_t, ThreadContextPtr&) const;
    void determine_chunking_strategy();
    double work_done_amount() const;
    void realloc_output_columns(size_t i, size_t new_nrows);
    void order_chunk(ChunkCoordinates& acc, ChunkCoordinates& xcc,
                     ThreadContextPtr& ctx);
};



}}  // namespace dt::read
#endif
