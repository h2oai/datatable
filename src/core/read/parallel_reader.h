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
#ifndef dt_READ_PARALLELREADER_h
#define dt_READ_PARALLELREADER_h
#include "read/chunk_coordinates.h"  // ChunkCoordinates
#include "read/thread_context.h"     // ThreadContext
#include "parallel/api.h"
#include "_dt.h"
namespace dt {
namespace read {


/**
  * This class' responsibility is to execute parallel reading of its
  * input, ensuring that the data integrity is maintained.
  */
class ParallelReader
{
  protected:
    size_t chunk_size;
    size_t chunk_count;
    const char* input_start;
    const char* input_end;
    const char* end_of_last_chunk;
    double approximate_line_length;

  protected:
    GenericReader& g;
    PreFrame& preframe;
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
        ChunkCoordinates&, ThreadContext*) const {}

    /**
     * Return an instance of a `ThreadContext` class. Implementations of
     * `ParallelReader` are expected to override this method to return
     * appropriate subclasses of `ThreadContext`.
     */
    virtual std::unique_ptr<ThreadContext> init_thread_context() = 0;

  private:
    ChunkCoordinates compute_chunk_boundaries(size_t, ThreadContext*) const;
    void determine_chunking_strategy();
    double work_done_amount() const;
};



}}  // namespace dt::read
#endif
