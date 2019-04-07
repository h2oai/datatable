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
#include "utils/exceptions.h"
#include "utils/parallel.h"
#include "options.h"

namespace dt {



//------------------------------------------------------------------------------
// ordered_job
//------------------------------------------------------------------------------

ordered_job::ordered_job(size_t n, bool force_single_threaded)
  : nrows(n),
    noomp(force_single_threaded) {}

ordered_job::~ordered_job() {}

ojcontext::~ojcontext() {}


ojcptr ordered_job::start_thread_context() {
  return ojcptr();
}

void ordered_job::finish_thread_context(ojcptr& ctx) {
  run(ctx, nrows, nrows);
}


void ordered_job::execute() {
  constexpr size_t min_nrows_per_thread = 100;
  size_t nthreads = noomp? 0 : nrows / min_nrows_per_thread;
  size_t nchunks = 1 + (nrows - 1)/1000;
  size_t chunksize = 1 + (nrows - 1)/nchunks;

  dt::parallel_for_ordered(
    nchunks,
    nthreads,  // will be truncated to pool size if necessary
    [&](ordered* o) {
      ojcptr ctx = start_thread_context();

      o->parallel(
        [&](size_t j) {
          size_t i0 = std::min(j * chunksize, nrows);
          size_t i1 = std::min(i0 + chunksize, nrows);
          run(ctx, i0, i1);
        },
        [&](size_t) { order(ctx); },
        nullptr
      );

      finish_thread_context(ctx);
    });
}



//------------------------------------------------------------------------------
// Ordered iteration, produce a string column
//------------------------------------------------------------------------------

Column* generate_string_column(function<void(size_t, string_buf*)> fn,
                               size_t nrows,
                               MemoryRange&& offsets_buffer,
                               bool force_str64,
                               bool force_single_threaded)
{
  constexpr size_t min_nrows_per_thread = 100;
  size_t nthreads = force_single_threaded? 0 : nrows / min_nrows_per_thread;
  size_t nchunks = 1 + (nrows - 1)/1000;
  size_t chunksize = 1 + (nrows - 1)/nchunks;

  writable_string_col outcol(std::move(offsets_buffer), nrows, force_str64);

  dt::parallel_for_ordered(
    nchunks,
    nthreads,  // will be truncated to pool size if necessary
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
        [&](size_t) { sb->order(); },
        nullptr
      );

      sb->commit_and_start_new_chunk(nrows);
    });

  return std::move(outcol).to_column();
}


}  // namespace dt
