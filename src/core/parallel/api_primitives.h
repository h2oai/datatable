//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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
#ifndef dt_PARALLEL_API_PRIMITIVES_h
#define dt_PARALLEL_API_PRIMITIVES_h
#include <cstddef>
#include "utils/assert.h"
namespace dt {
using std::size_t;

size_t num_threads_in_pool();



class ChunkSize {
  size_t value;
  public:
    explicit ChunkSize(size_t sz = 1000 /* Default chunk size */) {
      value = sz? sz : 1;
    }
    ChunkSize(const ChunkSize&) = default;

    size_t get() const noexcept {
      return value;
    }
};


class NThreads {
  private:
    size_t value;

  public:
    explicit NThreads() : value(num_threads_in_pool()) {}
    explicit NThreads(int nth) : NThreads(static_cast<size_t>(nth)) {}
    explicit NThreads(size_t nth) {
      size_t n0 = num_threads_in_pool();
      value = (nth == 0 || nth >= n0) ? n0 : nth;
    }
    explicit NThreads(bool parallel_ok) {
      value = parallel_ok? num_threads_in_pool() : 1;
    }

    NThreads(const NThreads&) = default;

    size_t get() const {
      xassert(value > 0 && value <= num_threads_in_pool());
      return value;
    }
};



/**
 * Create NThreads object with the number of threads calculated
 * from the number of iterations and the minimum number of
 * iterations per thread.
 */
NThreads nthreads_from_niters(size_t niters,
                              size_t min_iters_per_thread = 1000,
                              bool parallel_ok = true);


}  // namespace dt
#endif
