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
#include <functional>   // std::function
#include <memory>       // std::unique_ptr


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



}  // namespace dt
#endif
