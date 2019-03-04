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
#include "utils/c+++.h"
#include "utils/function.h"
#include "rowindex.h"
#include "wstringcol.h"

#ifdef DTNOOPENMP
  #define omp_get_max_threads() 1
  #define omp_get_num_threads() 1
  #define omp_set_num_threads(n)
  #define omp_get_thread_num() 0
#else
  #ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wreserved-id-macro"
    #include <omp.h>
    #pragma clang diagnostic pop
  #else
    #include <omp.h>
  #endif
#endif

namespace dt {


//------------------------------------------------------------------------------
// Order-less iteration over range [0; n)
//------------------------------------------------------------------------------

using rangefn = dt::function<void(size_t, size_t, size_t)>;

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
void run_parallel(rangefn run, size_t nrows);




//------------------------------------------------------------------------------
// Ordered iteration over range [0; n)
//------------------------------------------------------------------------------

class ojcontext {};
using ojcptr = std::unique_ptr<ojcontext>;

class ordered_job {
  public:
    size_t nrows;

    ordered_job(size_t n);
    virtual ~ordered_job();
    virtual ojcptr start_thread_context();
    virtual void run(ojcptr& ctx, size_t i0, size_t i1) = 0;
    virtual void order(ojcptr& ctx) = 0;
    virtual void finish_thread_context(ojcptr& ctx);

    /**
     * Run a job over the range `[0 .. nrows - 1]` in an ordered manner.
     * Specifically, each thread will:
     *   (1) create a new `ojcontext` object using the `start_thread_context()`
     *       method;
     *   (2) split the range `[0 .. nrows - 1]` into a sequence of chunks;
     *   (3) execute `run(ctx, start, end)` method, in parallel;
     *   (4) execute `order(ctx)` method within the "omp ordered" section,
     *       meaning that only one thread at a time will be executing this method,
     *       and in the order of the chunks.
     *   (5) at the end of the iteration, each thread will call
     *       `finish_thread_context(ctx)`, giving the threads a chance
     *       to perform any necessary cleanup.
     *
     * This function is best suited for those cases when the processing has to run
     * as-if sequentially. For example, writing or modifying a string column.
     */
    void execute();
};




//------------------------------------------------------------------------------
// Iterate over range [0; n), producing a string column
//------------------------------------------------------------------------------

using string_buf = writable_string_col::buffer;

Column* generate_string_column(dt::function<void(size_t, string_buf*)> fn,
                               size_t n,
                               bool str64 = false);

Column* generate_string_column(dt::function<void(size_t, string_buf*)> fn,
                               size_t n,
                               MemoryRange&& offsets_buffer,
                               bool str64 = false);




//------------------------------------------------------------------------------
// Map over a string column producing a new ostring column
//------------------------------------------------------------------------------

template <typename T, typename F>
class mapper_str2str : private ordered_job {
  private:
    StringColumn<T>* inpcol;
    writable_string_col outcol;
    F f;

    struct thcontext : public ojcontext {
      std::unique_ptr<string_buf> sb;
      thcontext (writable_string_col& ws)
        : sb(make_unique<writable_string_col::buffer_impl<uint32_t>>(ws)) {}
    };

  public:
    mapper_str2str(StringColumn<T>* col, F _f)
      : ordered_job(col->nrows),
        inpcol(col),
        outcol(col->nrows),
        f(_f) {}
    ~mapper_str2str() override = default;

    Column* result() {
      execute();
      return std::move(outcol).to_column();
    }

  private:
    ojcptr start_thread_context() override {
      return ojcptr(new thcontext(outcol));
    }

    void run(ojcptr& ctx, size_t i0, size_t i1) override {
      auto sb = static_cast<thcontext*>(ctx.get())->sb.get();
      sb->commit_and_start_new_chunk(i0);
      CString curr_str;
      const T* offsets = inpcol->offsets();
      const char* strdata = inpcol->strdata();
      const RowIndex& rowindex = inpcol->rowindex();
      for (size_t i = i0; i < i1; ++i) {
        size_t j = rowindex[i];
        if (j == RowIndex::NA || ISNA<T>(offsets[j])) {
          curr_str.ch = nullptr;
          curr_str.size = 0;
        } else {
          T offstart = offsets[j - 1] & ~GETNA<T>();
          T offend = offsets[j];
          curr_str.ch = strdata + offstart;
          curr_str.size = static_cast<int64_t>(offend - offstart);
        }
        f(j, curr_str, sb);
      }
    }

    void order(ojcptr& ctx) override {
      static_cast<thcontext*>(ctx.get())->sb->order();
    }
};


template <typename T, typename F>
Column* map_str2str(StringColumn<T>* col, F f) {
  return mapper_str2str<T, F>(col, f).result();
}

}  // namespace dt
#endif
