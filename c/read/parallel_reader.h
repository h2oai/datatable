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



/**
 * This class' responsibility is to execute parallel reading of its input,
 * ensuring that the data integrity is maintained.
 */
class ParallelReader {
  using ThreadContextPtr = std::unique_ptr<ThreadContext>;

  protected:
    size_t chunkSize;
    size_t chunkCount;
    const char* inputStart;
    const char* inputEnd;
    const char* lastChunkEnd;
    double lineLength;

  protected:
    GenericReader& g;
    dt::shared_mutex shmutex;
    size_t nrows_max;
    size_t nrows_allocated;
    size_t nrows_written;
    int nthreads;
    int : 32;

  public:
    ParallelReader(GenericReader& reader, double len);
    ParallelReader(const ParallelReader&) = delete;
    ParallelReader& operator=(const ParallelReader&) = delete;
    virtual ~ParallelReader() {}

    /**
     * Main function that reads all data from the input.
     */
    virtual void read_all();


  protected:
    /**
     * Determine coordinates (start and end) of the `i`-th chunk. The index `i`
     * must be in the range [0..chunkCount).
     *
     * The optional `ThreadContext` instance may be needed for some
     * implementations of `ParallelReader` in order to perform additional
     * parsing using a thread-local context.
     *
     * The method `compute_chunk_boundaries()` may be called in-parallel,
     * assuming that different invocation receive different `ctx` objects.
     */
    ChunkCoordinates compute_chunk_boundaries(
      size_t i, ThreadContext* ctx) const;

    /**
     * This method can be overridden in derived classes in order to implement
     * more advanced chunk boundaries detection. This method will be called from
     * within `compute_chunk_boundaries()` only.
     * This method should modify `cc` by-reference; making sure not to alter
     * `start` / `end` if the flags `start_exact` / `end_exact` are set.
     */
    virtual void adjust_chunk_coordinates(
      ChunkCoordinates& cc, ThreadContext* ctx) const;

    /**
     * Return an instance of a `ThreadContext` class. Implementations of
     * `ParallelReader` are expected to override this method to return
     * appropriate subclasses of `ThreadContext`.
     */
    virtual ThreadContextPtr init_thread_context() = 0;


  private:
    void determine_chunking_strategy();

    /**
     * Return the fraction of the input that was parsed, as a number between
     * 0 and 1.0.
     */
    double work_done_amount() const;

    /**
     * Reallocate output columns (i.e. `g.columns`) to the new number of rows.
     * Argument `i` contains the index of the chunk that was read last (this
     * helps with determining the new number of rows), and `new_allocnrow` is
     * the minimal number of rows to reallocate to.
     *
     * This method is thread-safe.
     */
    void realloc_output_columns(size_t i, size_t new_allocnrow);

    /**
     * Ensure that the chunks were placed properly. This method must be called
     * from the #ordered section. It takes three arguments: `acc` the *actual*
     * coordinates of the chunk just read; `xcc` the coordinates that were
     * *expected*; and `ctx` the thread-local parse context.
     *
     * If the chunk was ordered properly (i.e. started reading from the place
     * were the previous chunk ended), then this method updates the internal
     * `lastChunkEnd` variable and returns.
     *
     * Otherwise, it re-parses the chunk with correct coordinates. When doing
     * so, it will set `xcc.start_exact` to true, thus informing the chunk
     * parser that the coordinates that it received are true.
     */
    void order_chunk(ChunkCoordinates& acc, ChunkCoordinates& xcc,
                     ThreadContextPtr& ctx);
};



}}  // namespace dt::read
#endif
