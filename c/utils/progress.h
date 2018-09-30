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
#ifndef dt_UTILS_PROGRESS_h
#define dt_UTILS_PROGRESS_h
#include <functional>   // std::function
#include <memory>       // std::unique_ptr

namespace dt {


/**
 * Execute function `run` in parallel, across the range `[0 .. nrows - 1]`.
 * The signature of function `run` is that of a "range" function:
 * `(start, end, step)`. The function `run` is then expected to run a loop for
 * the indices in this range.
 *
 * Each thread will thus run on a set of rows that are at a constant distance
 * apart. This pattern of execution is most appropriate when:
 *   - The amount of work per row is relatively small;
 *   - The rows can be processed in any order.
 */
void run_interleaved(size_t nrows,
                     std::function<void(size_t&, size_t, size_t)> run);


class OrderedJobContext {
  public:
    virtual void run(size_t istart, size_t iend) = 0;
    virtual void commit() = 0;
    virtual ~OrderedJobContext() = 0;
};
using ojcptr = std::unique_ptr<OrderedJobContext>;


/**
 * Run a job over the range `[0 .. nrows - 1]` in an ordered manner.
 * Specifically, each thread will:
 *   (1) create a new `OrderedJobContext` object using the provided `prepare`
 *       function (which takes the threan number and the total number of threads
 *       as the parameters);
 *   (2) split the range `[0 .. nrows - 1]` into a sequence of chunks;
 *   (3) execute `ojc->run(start, end)` method, in parallel;
 *   (4) execute `ojc->commit()` method within the "omp ordered" section,
 *       meaning that only one thread at a time will be executing this method,
 *       and in the order of the chunks.
 *
 * This function is best suited for those cases when the processing has to run
 * as-if sequentially. For example, writing or modifying a string column.
 */
void run_ordered(size_t nrows,
                 std::function<ojcptr(int, int)> prepare);


}

#endif
