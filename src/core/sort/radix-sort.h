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
#ifndef dt_SORT_RADIX_SORT_h
#define dt_SORT_RADIX_SORT_h
#include <functional>        // std::function
#include <tuple>             // std::tuple
#include <type_traits>       // std::is_convertible
#include "parallel/api.h"    // num_threads_in_pool, parallel_for_static
#include "sort/common.h"
#include "utils/assert.h"    // xassert
namespace dt {
namespace sort {


static constexpr size_t MIN_NROWS_PER_THREAD = 1024;


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
class RadixSort {
  private:
    size_t n_radixes_;
    size_t n_rows_;
    size_t n_chunks_;
    size_t n_rows_per_chunk_;
    Buffer histogram_buffer_;

  public:
    RadixSort(size_t nrows, int nrb, bool allow_parallel) {
      xassert(nrows > 0);
      xassert(nrb > 0 && nrb <= 20);
      n_radixes_ = (1u << nrb) + 1;
      n_rows_ = nrows;
      // If `allow_parallel` is false, then setting `n_chunks_` to 1
      // will ensure that the parallel constructs such as
      // dt::parallel_for_static will not actually spawn a parallel
      // region.
      n_chunks_ = 1;
      if (allow_parallel) {
        xassert(dt::num_threads_in_team() == 0);
        n_chunks_ = std::min(dt::num_threads_in_pool(),
                            divceil(nrows, MIN_NROWS_PER_THREAD));
      }
      n_rows_per_chunk_ = divceil(nrows, n_chunks_);
    }


    /**
      * .sort_by_radix(ordering_in, ordering_out, get_radix,
      *                [move_data])
      *
      * First step of the radix-sort algorithm. In this step we sort
      * the indices in vector `ordering_in` and write the result into
      * `ordering_out`. The sorting keys are the values returned by
      * the `get_radix` function.
      *
      * The array `ordering_in` may also be empty, which is equivalent
      * to it being {0, 1, 2, ..., n_rows_-1}.
      *
      * The return value is the "grouping" array, i.e. the array of
      * offsets (within the `ordering_out` array) that define the
      * groups of data. Some of those groups may be empty. The size
      * of the grouping array is equal to the number of radixes, and
      * its lifetime is tied to the lifetime of the RadixSort object.
      *
      * The function `get_radix` has the signature `(size_t)->size_t`,
      * it takes an index as an argument, and must return the radix
      * of the value at that index. The value of the radix cannot
      * exceed `1 << nradixbits` (though it can be equal).
      *
      * The optional argument `move_data` is a function with a
      * signature `(size_t, size_t)->void`. This function will be
      * called once for every input observation, with two arguments:
      * the index of an input observation, and the index of the same
      * observation in the sorted sequence. The caller can use this
      * to store the sorted data.
      */
    template <typename TO, typename GetRadix, typename MoveData>
    array<TO> sort_by_radix(array<TO> ordering_in,
                            array<TO> ordering_out,
                            GetRadix get_radix,
                            MoveData move_data)
    {
      static_assert(
        std::is_convertible<GetRadix, std::function<size_t(size_t)>>::value,
        "Incorrect signature of get_radix function");
      static_assert(
        std::is_convertible<MoveData,
                            std::function<void(size_t,size_t)>>::value,
        "Incorrect signature of move_data function");
      xassert(ordering_in.size() == n_rows_ || ordering_in.size() == 0);
      xassert(ordering_out.size() == n_rows_);

      array<TO> histogram = allocate_histogram<TO>();
      build_histogram(histogram, get_radix);
      if (ordering_in.size()) {
        reorder_data(histogram, get_radix,
                     [&](size_t i, size_t j) {
                       ordering_out[j] = ordering_in[i];
                       move_data(i, j);
                     });
      } else {
        reorder_data(histogram, get_radix,
                     [&](size_t i, size_t j) {
                       ordering_out[j] = static_cast<TO>(i);
                       move_data(i, j);
                     });
      }
      return array<TO>(histogram.ptr() + (n_chunks_ - 1) * n_radixes_,
                       n_radixes_);
    }

    // (same method, but with `move_data` argument omitted)
    template <typename TO, typename GetRadix>
    array<TO> sort_by_radix(array<TO> ordering_in,
                            array<TO> ordering_out, GetRadix get_radix)
    {
      return sort_by_radix(ordering_in, ordering_out, get_radix,
                           [](size_t, size_t){});
    }


    /**
      * sort_subgroups(groups, ordering_in, ordering_out, subsort)
      *
      * Second step in the radix-sort algorithm. This step takes the
      * array of group offsets `groups` (same array as returned from
      * `sort_by_radix()`), and sorts each of these groups using the
      * function `subsort`.
      */
    template <typename TO, typename Subsort>
    void sort_subgroups(array<TO> groups,
                        array<TO> ordering_in,
                        array<TO> ordering_out,
                        Subsort subsort)
    {
      static_assert(
        std::is_convertible<Subsort,
                            std::function<void(size_t,size_t,array<TO>,array<TO>,bool)>>::value,
        "Invalid signature of subsort function");
      xassert(groups.size() > 0);
      xassert(ordering_in.size() == n_rows_ && ordering_out.size() == n_rows_);

      if (n_chunks_ == 1 || true) {
        const size_t ngroups = groups.size();
        size_t group_start = 0;
        for (size_t i = 0; i < ngroups; ++i) {
          size_t group_end = static_cast<size_t>(groups[i]);
          size_t group_size = group_end - group_start;
          if (group_size > 1) {
            subsort(group_start, group_size,
                    ordering_in.subset(group_start, group_size),
                    ordering_out.subset(group_start, group_size), true);
          }
          group_end = group_start;
        }
      }
      // TODO: use parallelism

      // const size_t large_group = std::max(n_rows_ / 4, size_t(100000));
      // size_t group_start = 0;
      // for (size_t i = 0; i < ngroups; ++i) {
      //   size_t group_end = groups[i];
      //   size_t group_size = group_end - group_start;
      // }

      //   Buffer tmp;

      //   size_t group_start = 0;
      //   size_t first_group = 0;
      //   size_t last_group = nradixes_;
      //   for (size_t i = 0; i < nradixes_; ++i) {
      //     size_t group_end = group_offsets[i];
      //     if (group_end == 0) { first_group = i + 1; continue; }
      //     size_t group_size = group_end - group_start;
      //     if (group_size >= large_group) {
      //       tmp.ensuresize(group_size * sizeof(TO));
      //       TO* subordering = static_cast<TO*>(tmp.xptr());
      //       sort_subgroup(group_start, group_size, subordering);
      //       reorder_indices(ordering_out + group_start, group_size,
      //                       subordering);
      //     }
      //     if (group_end == n) { last_group = i + 1; break;}
      //     group_start = group_end;
      //   }

    }


  //----------------------------------------------------------------------------
  // Private implementation
  //----------------------------------------------------------------------------
  private:
    std::tuple<size_t, size_t> get_chunk(size_t i) const {
      xassert(i < n_chunks_);
      size_t start = i * n_rows_per_chunk_;
      size_t end = start + n_rows_per_chunk_;
      if (i == n_chunks_ - 1) end = n_rows_;
      return std::make_tuple(start, end);
    }

    template <typename TH>
    array<TH> allocate_histogram() {
      xassert(n_rows_ <= MAX_NROWS_INT32 || sizeof(TH) == 8);
      histogram_buffer_.resize(n_chunks_ * n_radixes_ * sizeof(TH), false);
      return array<TH>(histogram_buffer_);
    }

    /**
      * Calculate the histograms of radixes in the column being
      * sorted.
      * Specifically, we're creating the `histogram` table which has
      * `n_chunks_` rows and `n_radixes_` columns. Cell [i,j] in this
      * table will contain the count of radixes `j` within the chunk
      * `i`. After that the values are cumulated across all `j`s
      * (i.e. in the end the histogram will contain cumulative counts
      * of values in the sorted column).
      */
    template <typename TH, typename GetRadix>
    void build_histogram(array<TH> histogram, GetRadix get_radix) {
      dt::parallel_for_static(
        n_chunks_,          // n iterations
        dt::ChunkSize(1),  // each iteration processed individually
        [&](size_t i) {
          TH* tcounts = histogram.ptr() + (n_radixes_ * i);
          size_t j0, j1; std::tie(j0, j1) = get_chunk(i);
          for (size_t j = j0; j < j1; ++j) {
            size_t radix = get_radix(j);
            xassert(radix < n_radixes_);
            tcounts[radix]++;
          }
        });
      cumulate_histogram<TH>(histogram);
    }

    template <typename TH>
    void cumulate_histogram(array<TH> histogram) {
      size_t cumsum = 0;
      size_t histogram_size = n_chunks_ * n_radixes_;
      for (size_t j = 0; j < n_radixes_; ++j) {
        for (size_t r = j; r < histogram_size; r += n_radixes_) {
          TH t = histogram[r];
          histogram[r] = static_cast<TH>(cumsum);
          cumsum += static_cast<size_t>(t);
        }
      }
    }



    template<typename TH, typename GetRadix, typename MoveData>
    void reorder_data(array<TH> histogram,
                      GetRadix get_radix,
                      MoveData move_data)
    {
      dt::parallel_for_static(
        n_chunks_,
        dt::ChunkSize(1),
        [&](size_t i) {
          TH* tcounts = histogram.ptr() + (n_radixes_ * i);
          size_t j0, j1; std::tie(j0, j1) = get_chunk(i);
          for (size_t j = j0; j < j1; ++j) {
            size_t radix = get_radix(j);
            TH k = tcounts[radix]++;
            xassert(static_cast<size_t>(k) < n_rows_);
            move_data(j, static_cast<size_t>(k));
          }
        });
      xassert(static_cast<size_t>(histogram[histogram.size() - 1]) == n_rows_);
    }


};




}}  // namespace dt::sort
#endif
