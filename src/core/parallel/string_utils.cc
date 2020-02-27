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
#include "column/sentinel_str.h"
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
  if (nrows == 0) {
    return force_str64? Column(new SentinelStr_ColumnImpl<uint64_t>(0))
                      : Column(new SentinelStr_ColumnImpl<uint32_t>(0));
  }
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
