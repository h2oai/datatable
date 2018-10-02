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
#ifndef dt_UTILS_PARALLEL_h
#define dt_UTILS_PARALLEL_h
#include <functional>     // std::function
#include <memory>         // std::unique_ptr
#include "wstringcol.h"


#ifdef DTNOOPENMP
  #define omp_get_max_threads() 1
  #define omp_get_num_threads() 1
  #define omp_set_num_threads(n)
  #define omp_get_thread_num() 0
#else
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wreserved-id-macro"
  #include <omp.h>
  #pragma clang diagnostic pop
#endif


namespace dt {

using rangefn = std::function<void(size_t, size_t, size_t)>;

/**
 * Execute function `run` in parallel, over the range `[0 .. nrows - 1]`.
 * The signature of function `run` is that of the "range" function:
 * `(start, end, step)`. The function `run` is then expected to run a loop for
 * the indices in this range.
 *
 * Each thread will thus run on a set of rows that are at a constant distance
 * apart. This pattern of execution is most appropriate when:
 *   - The amount of work per row is relatively small;
 *   - The rows can be processed in any order.
 */
void run_interleaved(rangefn run, size_t nrows);






class ojcontext {};
using ojcptr = std::unique_ptr<ojcontext>;

class ordered_job {
  public:
    size_t nrows;

    ordered_job(size_t n);
    virtual ~ordered_job();
    virtual ojcptr make_thread_context() = 0;
    virtual void run(ojcptr& ctx, size_t i0, size_t i1) = 0;
    virtual void order(ojcptr& ctx) = 0;

    /**
     * Run a job over the range `[0 .. nrows - 1]` in an ordered manner.
     * Specifically, each thread will:
     *   (1) create a new `OrderedJobContext` object using the provided `prepare`
     *       function (which takes the threan number and the total number of threads
     *       as the parameters);
     *   (2) split the range `[0 .. nrows - 1]` into a sequence of chunks;
     *   (3) execute `ojc->run(start, end)` method, in parallel;
     *   (4) execute `ojc->order()` method within the "omp ordered" section,
     *       meaning that only one thread at a time will be executing this method,
     *       and in the order of the chunks.
     *
     * This function is best suited for those cases when the processing has to run
     * as-if sequentially. For example, writing or modifying a string column.
     */
    void execute();
};



template <void (*F)(size_t, fixed_height_string_col::buffer&)>
class map_fw2str : private ordered_job {
  private:
    fixed_height_string_col col;

    struct thcontext : private ojcontext {
      fixed_height_string_col::buffer sb;
      thcontext (fixed_height_string_col& fhsc) : sb(fhsc) {}
    };

  public:
    map_fw2str(size_t nrows) : ordered_job(nrows), col(nrows) {
      execute();
    }
    ~map_fw2str() = default;

  private:
    ojcptr make_thread_context() override {
      return ojcptr(new thcontext(col));
    }

    void run(ojcptr& ctx, size_t i0, size_t i1) override {
      auto& sb = static_cast<thcontext*>(ctx.get())->sb;
      sb.commit_and_start_new_chunk(i0);
      for (size_t i = i0; i < i1; ++i) {
        F(i, sb);
      }
    }

    void order(ojcptr& ctx) override {
      auto& sb = static_cast<thcontext*>(ctx.get())->sb;
      sb.order();
    }
};


}  // namespace dt
#endif
