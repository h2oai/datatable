//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
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
#ifndef dt_SORT_RADIXSORT_h
#define dt_SORT_RADIXSORT_h
#include <tuple>             // std::tuple
#include "parallel/api.h"    // num_threads_in_pool, parallel_for_static
#include "sort/common.h"
#include "utils/assert.h"    // xassert
namespace dt {
namespace sort {


static constexpr size_t MIN_NROWS_PER_THREAD = 1024;
static constexpr size_t MAX_NROWS_INT32 = 0x7FFFFFFF;


// Compute ceil(a/b), for integer a and b.
template <typename T>
inline T divceil(T a, T b) {
  return (a - 1) / b + 1;
}


/**
  * Helper class that holds all the parameters needed during radix
  * sort. For convenience, the members in this class are readable
  * directly (not encapsulated).
  */
class RadixConfig {
  public:
    size_t n_radixes;
    size_t n_radix_bits;
    size_t n_rows;
    size_t n_chunks;
    size_t n_rows_per_chunk;

    Buffer histogram_buffer();

  public:
    RadixConfig(size_t nrows, int nrb, bool allow_parallel) {
      xassert(nrb > 0 && nrb <= 20);
      n_radixes = (1u << nrb) + 1;
      n_radix_bits = static_cast<size_t>(nrb);
      n_rows = nrows;
      n_chunks = 1;
      if (allow_parallel) {
        xassert(dt::num_threads_in_team() == 0);
        n_chunks = std::min(dt::num_threads_in_pool(),
                            divceil(nrows, MIN_NROWS_PER_THREAD));
      }
      n_rows_per_chunk = divceil(nrows, n_chunks);
    }


    template <typename TO, typename F>
    array<TO> sort_by_radix(array<TO> ordering_out, F fn_get_radix) {
      xassert(ordering_out.size == n_rows);
      auto histogram = allocate_histogram<TO>();
      build_histogram(histogram, fn_get_radix);
      reorder_data(histogram, ordering_out, fn_get_radix);
      return array<TO>(histogram + (n_chunks - 1) * n_radixes, n_radixes);
    }


  private:
    std::tuple<size_t, size_t> get_chunk(size_t i) const {
      xassert(i < n_chunks);
      size_t start = i * n_rows_per_chunk;
      size_t end = start + n_rows_per_chunk;
      if (i == n_chunks - 1) end = n_rows;
      return std::make_tuple(start, end);
    }

    /**
      * Calculate the histograms of radixes in the column being
      * sorted.
      * Specifically, we're creating the `histogram_` table which has
      * `ndata_chunks_` rows and `nradix` columns. Cell `[i,j]` in
      * this table will contain the count of radixes `j` within the
      * chunk `i`. After that the values are cumulated across all
      * `j`s (i.e. in the end the histogram will contain cumulative
      * counts of values in the sorted column).
      */
    template <typename TH, typename F>
    void build_histogram(array<TH> histogram, F fn_get_radix) {
      dt::parallel_for_static(
        n_chunks,          // n iterations
        dt::ChunkSize(1),  // each iteration processed individually
        [&](size_t i) {
          TH* tcounts = histogram.ptr + (n_radixes * i);
          size_t j0, j1; std::tie(j0, j1) = get_chunk(i);
          for (size_t j = j0; j < j1; ++j) {
            size_t radix = fn_get_radix(j);
            xassert(radix < n_radixes);
            tcounts[radix]++;
          }
        });
      cumulate_histogram(histogram);
    }

    template <typename TH>
    array<TH> allocate_histogram() {
      xassert(nrows <= MAX_NROWS_INT32 || sizeof(TH) == 8);
      size_t histogram_size = n_chunks * n_radixes;
      histogram_buffer.resize(histogram_size * sizeof(TH), false);
      return array<TH>(histogram_buffer);
    }

    template <typename TH>
    void cumulate_histogram(array<TH> histogram) {
      size_t cumsum = 0;
      size_t histogram_size = n_chunks * n_radixes;
      for (size_t j = 0; j < n_radixes; ++j) {
        for (size_t r = j; r < histogram_size; r += n_radixes) {
          auto t = histogram.ptr[r];
          histogram.ptr[r] = cumsum;
          cumsum += t;
        }
      }
    }



    template<typename TH, typename TO, typename F>
    void reorder_data(array<TH> histogram, array<TO> ordering_out,
                      F fn_get_radix)
    {
      xassert(ordering_out.size == n_rows);
      dt::parallel_for_static(
        n_chunks,
        dt::ChunkSize(1),
        [&](size_t i) {
          TH* tcounts = histogram.ptr + (n_radixes * i);
          size_t j0, j1; std::tie(j0, j1) = get_chunk(i);
          for (size_t j = j0; j < j1; ++j) {
            size_t radix = fn_get_radix(j);
            size_t k = tcounts[radix]++;
            xassert(k < n_rows);
            ordering_out.ptr[k] = static_cast<TO>(j);
          }
        });
      xassert(histogram.ptr[n_chunks * n_radixes - 1] == n_rows);
    }


};




}}  // namespace dt::sort
#endif
