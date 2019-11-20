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
#include "parallel/api.h"
#include "parallel/string_utils.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "options.h"

namespace dt {


//------------------------------------------------------------------------------
// Ordered iteration, produce a string column
//------------------------------------------------------------------------------

Column generate_string_column(function<void(size_t, string_buf*)> fn,
                              size_t nrows,
                              Buffer&& offsets_buffer,
                              bool force_str64,
                              bool force_single_threaded)
{
  size_t nchunks = 1 + (nrows - 1)/1000;
  size_t chunksize = 1 + (nrows - 1)/nchunks;

  writable_string_col outcol(std::move(offsets_buffer), nrows, force_str64);

  constexpr size_t min_nrows_per_thread = 100;
  NThreads nthreads = force_single_threaded
    ? NThreads(1)
    : nthreads_from_niters(nchunks, min_nrows_per_thread);

  dt::parallel_for_ordered(
    nchunks,
    nthreads,
    [&](ordered* o) {
      using sbptr = std::unique_ptr<string_buf>;
      auto sb = force_str64
        ? sbptr(new writable_string_col::buffer_impl<uint64_t>(outcol))
        : sbptr(new writable_string_col::buffer_impl<uint32_t>(outcol));

      o->parallel(
        [&](size_t j) {
          size_t i0 = std::min(j * chunksize, nrows);
          size_t i1 = std::min(i0 + chunksize, nrows);

          sb->commit_and_start_new_chunk(i0);
          for (size_t i = i0; i < i1; ++i) {
            fn(i, sb.get());
          }
        },
        [&](size_t) {
          sb->order();
        },
        nullptr
      );

      sb->commit_and_start_new_chunk(nrows);
    });

  return std::move(outcol).to_ocolumn();
}


}  // namespace dt
