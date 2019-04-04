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
#include "parallel/api.h"
#include "utils/assert.h"
#include "utils/function.h"
namespace dt {


//------------------------------------------------------------------------------
// parallel_for_static
//------------------------------------------------------------------------------

void _parallel_for_static(size_t nrows, size_t min_chunk_size,
                          function<void(size_t, size_t)> fn)
{
  size_t k = nrows / min_chunk_size;

  size_t ith = dt::this_thread_index();

  // Standard parallel loop
  if (ith == size_t(-1)) {
    if (k == 0) {
      fn(0, nrows);
    }
    else {
      size_t nth = num_threads_in_pool();
      size_t chunksize = nrows / k;
      size_t nchunks = nrows / chunksize;

      dt::parallel_region(
        [=] {
          size_t ithread = this_thread_index();
          for (size_t j = ithread; j < nchunks; j += nth) {
            size_t i0 = j * chunksize;
            size_t i1 = i0 + chunksize;
            if (j == nchunks - 1) i1 = nrows;
            fn(i0, i1);
          }
        });
    }
  }
  // Parallel loop within a parallel region
  else {
    if (k == 0) {
      if (ith == 0) fn(0, nrows);
    }
    else {
      size_t nth = num_threads_in_team();
      size_t chunksize = nrows / k;
      size_t nchunks = nrows / chunksize;

      for (size_t j = ith; j < nchunks; j += nth) {
        size_t i0 = j * chunksize;
        size_t i1 = i0 + chunksize;
        if (j == nchunks - 1) i1 = nrows;
        fn(i0, i1);
      }
    }
  }
}



} // namespace dt
