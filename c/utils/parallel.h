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
#include <iostream>
#include <functional>     // std::function
#include "utils/c+++.h"
#include "utils/function.h"
#include "parallel/api.h"
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
// Iterate over range [0; n), producing a string column
//------------------------------------------------------------------------------

using string_buf = writable_string_col::buffer;

Column* generate_string_column(dt::function<void(size_t, string_buf*)> fn,
                               size_t n,
                               MemoryRange&& offsets_buffer = MemoryRange(),
                               bool force_str64 = false,
                               bool force_single_threaded = false);




//------------------------------------------------------------------------------
// Map over a string column producing a new ostring column
//------------------------------------------------------------------------------


template <typename T, typename F>
Column* map_str2str(StringColumn<T>* input_col, F f) {
  size_t nrows = input_col->nrows;
  writable_string_col output_col(nrows);

  constexpr size_t min_nrows_per_thread = 100;
  size_t nthreads = nrows / min_nrows_per_thread;
  size_t nchunks = 1 + (nrows - 1)/1000;
  size_t chunksize = 1 + (nrows - 1)/nchunks;
  int k = 0;
  bool doing_ordered = false;
  size_t next_to_order = 0;

  std::cout << "calling parallel_for_ordered(nchunks=" << nchunks << ", nthreads=" << nthreads << ")\n";
  dt::parallel_for_ordered(
    nchunks,
    nthreads,  // will be truncated to pool size if necessary
    [&](ordered* o) {
      int khere = k++;
      std::cout << "Entered main lambda, with k=" << khere << "\n";
      std::unique_ptr<string_buf> sb(
          new writable_string_col::buffer_impl<uint32_t>(output_col));
      std::cout << '[' << khere << "] context = " << (sb.get()) << "\n";
      int state = 0;

      o->parallel(
        [&](size_t iter) {
          xassert(state == 0); state = 1;
          size_t i0 = std::min(iter * chunksize, nrows);
          size_t i1 = std::min(i0 + chunksize, nrows);

          sb->commit_and_start_new_chunk(i0);
          CString curr_str;
          const T* offsets = input_col->offsets();
          const char* strdata = input_col->strdata();
          const RowIndex& rowindex = input_col->rowindex();
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
            f(j, curr_str, sb.get());
          }

          xassert(state == 1); state = 2;
        },
        [&](size_t j) {
          xassert(!doing_ordered); doing_ordered = true;
          xassert(state == 2); state = 3;
          xassert(j == next_to_order); next_to_order++;
          sb->order();
          xassert(state == 3); state = 0;
          xassert(doing_ordered); doing_ordered = false;
        },
        nullptr
      );

      std::cout << '[' << khere << "] closing context " << (sb.get()) << "\n";
      sb->commit_and_start_new_chunk(nrows);
      std::cout << '[' << khere << "] leaving main lambda\n";
    });
  xassert(next_to_order == nchunks);

  return std::move(output_col).to_column();
}





}  // namespace dt
#endif
