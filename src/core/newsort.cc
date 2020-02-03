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
#include <tuple>             // std::make_tuple, std::tie
#include "parallel/api.h"    // parallel_for_static
#include "utils/assert.h"    // xassert
#include "buffer.h"          // Buffer
namespace dt {


class ChunkManager {
  private:
    size_t n_rows_;
    size_t n_chunks_;
    size_t n_rows_per_chunk_;

  public:
    ChunkManager(size_t nrows) {
      n_rows_ = nrows;
      n_chunks_ = 8;
      n_rows_per_chunk_ = (n_rows_ + n_chunks_ - 1) / n_chunks_;
    }

    size_t size() const {
      return n_chunks_;
    }

    std::tuple<size_t, size_t> get_chunk(size_t i) const {
      xassert(i < n_chunks_);
      size_t start = i * n_rows_per_chunk_;
      size_t end = start + n_rows_per_chunk_;
      if (i == n_chunks_ - 1) end = n_rows_;
      return std::make_tuple(start, end);
    }
};



/**
 * Class that wraps a column in the process of being sorted.
 */
class SorterColumn {
  private:
    ChunkManager chunks_;
    Buffer scratch_memory_;
    size_t nradixes_;


  //==========================================================================
  // Histograms (for radix sort)
  //==========================================================================
  public:

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
    void build_histogram() {
      size_t histogram_size = chunks_.size() * nradixes_;
      scratch_memory_.resize(histogram_size * sizeof(size_t), false);

      auto histogram = static_cast<size_t*>(scratch_memory_.xptr());
      _histogram_gather(histogram);
      _histogram_cumulate(histogram);
      return histogram;
    }


    /**
      * Can be overridden to supply a more specific implementation
      * of the `get_radix()` method.
      */
    virtual void _histogram_gather(size_t* histogram) {
      _histogram_impl(histogram,
                      [&](size_t j){ return get_radix(j); });
    }

    template <typename F>
    inline void _histogram_impl(size_t* histogram, F get_radix) {
      parallel_for_static(
        chunks_.size(),    // n iterations
        dt::ChunkSize(1),  // each iteration processed individually
        [&](size_t i) {
          size_t* tcounts = histogram + (nradixes_ * i);
          size_t j0, j1; std::tie(j0, j1) = chunks_.get_chunk(i);
          for (size_t j = j0; j < j1; ++j) {
            size_t radix = get_radix(j);
            xassert(radix < nradixes_);
            tcounts[radix]++;
          }
        });
    }


    void _histogram_cumulate(size_t* histogram) {
      size_t cumsum = 0;
      size_t histogram_size = chunks_.size() * nradixes_;
      for (size_t j = 0; j < nradixes_; ++j) {
        for (size_t r = j; r < histogram_size; r += nradixes_) {
          size_t t = histogram[r];
          histogram[r] = cumsum;
          cumsum += t;
        }
      }
    }

  public:
    /**
     * Return the number of rows in the column.
     */
    virtual size_t get_nrows() const = 0;


    /**
     * Return the number of bits this column should use for the next
     * step in radix sort procedure. The return value should be
     * non-negative. If you return 0, it means the data is already
     * sorted and no further reordering is necessary.
     *
     * The returned value will be used to allocate an array of size
     * proportional to (1 << get_radix_size()), which means you
     * shouldn't try to return values larger than 20 (and even this
     * can be too much).
     */
    virtual int get_radix_size() = 0;


    /**
     * Return the radix of the value at row `i`. The returned value
     * must be less than (1 << get_radix_size()).
     */
    virtual size_t get_radix(size_t i) const = 0;

};


template <typename T>
class Integer_SorteerColumn : public SorterColumn {
  using TU = typename std::make_unsigned<T>::type;
  using TM = typename std::common_type<T, int>::type;
  private:
    Column column_;
    int n_significant_bits_;
    int n_radix_bits_;
    TM  min1_;
    int shift_;

  public:
    Integer_SorteerColumn(const Column& col)
      : column_(col),
        n_significant_bits_(0),
        n_radix_bits_(0) {}

    size_t get_nrows() const override {
      return column_.nrows();
    }

    int get_radix_size() override {
      bool isvalid;
      int64_t min = column_.stats()->min_int(&isvalid);
      int64_t max = column_.stats()->max_int(&isvalid);
      if (isvalid && min != max) {
        n_significant_bits_ = sizeof(T) * 8;
        n_significant_bits_ -= dt::nlz(static_cast<TU>(max - min + 1));
      }
      n_radix_bits_ = std::min(8, n_significant_bits_);
      shift_ = n_significant_bits_ - n_radix_bits_;
      min1_ = static_cast<TM>(min - 1);
      return n_radix_bits_;
    }

    size_t get_radix(size_t i) const override {
      T value;
      bool isvalid = column_->get_element(i, &value);
      return isvalid? ((value - min1_) >> shift_) : 0;
    }
};



class SortExecutor {
  private:
    std::unique_ptr<SorterColumn> sorteer_;
    size_t nrows_;
    size_t nradixes_;
    size_t ndata_chunks_;
    size_t nrows_per_chunk_;

    size_t* histogram_;

  public:
    SortExecutor() {

    }

    template <typename TO>
    void reorder_data() {
      auto ordering_in = static_cast<const TO*>(input_ordering_buf_.rptr());
      auto ordering_out = static_cast<TO*>(output_ordering_buf_.xptr());

      parallel_for_static(ndata_chunks_, ChunkSize(1),
        [&](size_t i) {
          size_t* tcounts = histogram_ + (nradixes_ * i);
          size_t j0 = i * nrows_per_chunk_;
          size_t j1 = std::min(j0 + nrows_per_chunk_, nrows_);
          for (size_t j = j0; j < j1; ++j) {
            size_t radix = sorteer_->get_radix(j);
            size_t k = tcounts[radix]++;
            xassert(k < nrows_);
            ordering_out[k] = ordering_in[j];
            // if (OUT) {
            //   xo[k] = static_cast<TO>(xi[j] & mask);
            // }
          }
        });
      xassert(histogram_[ndata_chunks_ * nradixes_ - 1] == nrows_);
    }
};


}  // namespace dt
