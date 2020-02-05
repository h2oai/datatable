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
#include "frame/py_frame.h"  // py::Frame
#include "parallel/api.h"    // parallel_for_static
#include "python/args.h"     // py::PKArgs
#include "utils/assert.h"    // xassert
#include "utils/misc.h"      // dt::nlz
#include "buffer.h"          // Buffer
#include "column.h"          // Column
#include "rowindex.h"        // RowIndex
namespace dt {


class ChunkManager {
  private:
    size_t n_rows_;
    size_t n_chunks_;
    size_t n_rows_per_chunk_;

  public:
    ChunkManager()
      : n_rows_(0),
        n_chunks_(0),
        n_rows_per_chunk_(0) {}

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


static size_t nrows_insertsort = 16;
// static size_t nrows_quicksort = 256;


/**
 * Class that wraps a column in the process of being sorted.
 */
class SorterColumn {
  private:
    ChunkManager chunks_;
    Buffer scratch_memory_;
    size_t nradixes_;

  //--------------------------------------------------------------------------
  // Public interface
  //--------------------------------------------------------------------------
  public:
    SorterColumn() = default;
    virtual ~SorterColumn() = default;

    RowIndex sort() {
      size_t nrows = get_nrows();
      bool use_int32 = (nrows < std::numeric_limits<int32_t>::max());
      size_t elemsize = use_int32? sizeof(int32_t) : sizeof(int64_t);
      Buffer rowindex_buf = Buffer::mem(nrows * elemsize);


      if (nrows < nrows_insertsort) {
        auto ordering32 = static_cast<int32_t*>(rowindex_buf.xptr());
        insert_sort(ordering32, nrows);
      }
      // 1. Allocate objects
      // 2. Build histogram

      return RowIndex(std::move(rowindex_buf),
                      use_int32? RowIndex::ARR32 : RowIndex::ARR64);
    }


  //--------------------------------------------------------------------------
  // Insertion sort
  //--------------------------------------------------------------------------
  protected:

    virtual void insert_sort(int32_t* ordering, size_t nrows) = 0;

    template <typename F>
    static void insert_sort_impl(int32_t* ordering, size_t nrows,
                                 F compare_lt)
    {
      ordering[0] = 0;
      for (size_t i = 1; i < nrows; ++i) {
        size_t j = i;
        while (j && compare_lt(i, static_cast<size_t>(ordering[j - 1]))) {
          ordering[j] = ordering[j - 1];
          j--;
        }
        ordering[j] = static_cast<int32_t>(i);
      }
    }



  //--------------------------------------------------------------------------
  // Histograms (for radix sort)
  //--------------------------------------------------------------------------
  private:

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
    size_t* build_histogram() {
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

  protected:
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
     * must be less than or equal to (1 << get_radix_size()).
     */
    virtual size_t get_radix(size_t i) const = 0;

};


class Boolean_SorterColumn : public SorterColumn {
  private:
    Column column_;

  public:
    Boolean_SorterColumn(const Column& col) {
      xassert(col.stype() == SType::BOOL);
      column_ = col;
    }

  protected:
    size_t get_nrows() const override {
      return column_.nrows();
    }

    void insert_sort(int32_t* ordering, size_t n) override {
      insert_sort_impl(ordering, n,
        [&](size_t ia, size_t ib) {
          int8_t avalue, bvalue;
          bool avalid = column_.get_element(ia, &avalue);
          bool bvalid = column_.get_element(ib, &bvalue);
          return bvalid && (!avalid || avalue < bvalue);
        });
    }


    int get_radix_size() override {
      return 1;
    }

    size_t get_radix(size_t i) const override {
      int8_t value;
      bool isvalid = column_.get_element(i, &value);
      return isvalid? static_cast<size_t>(value + 1) : 0;
    }
};


/*
template <typename T>
class Integer_SorterColumn : public SorterColumn {
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
      bool isvalid = column_.get_element(i, &value);
      return isvalid? ((value - min1_) >> shift_) : 0;
    }
};
*/


/*
class SortExecutor {
  private:
    std::unique_ptr<SorterColumn> sortcol_;
    size_t nrows_;
    size_t nradixes_;
    size_t ndata_chunks_;
    size_t nrows_per_chunk_;

    size_t* histogram_;

  public:
    SortExecutor() {

    }

    // template <typename TO>
    // void reorder_data() {
    //   auto ordering_in = static_cast<const TO*>(input_ordering_buf_.rptr());
    //   auto ordering_out = static_cast<TO*>(output_ordering_buf_.xptr());

    //   parallel_for_static(ndata_chunks_, ChunkSize(1),
    //     [&](size_t i) {
    //       size_t* tcounts = histogram_ + (nradixes_ * i);
    //       size_t j0 = i * nrows_per_chunk_;
    //       size_t j1 = std::min(j0 + nrows_per_chunk_, nrows_);
    //       for (size_t j = j0; j < j1; ++j) {
    //         size_t radix = sorteer_->get_radix(j);
    //         size_t k = tcounts[radix]++;
    //         xassert(k < nrows_);
    //         ordering_out[k] = ordering_in[j];
    //         // if (OUT) {
    //         //   xo[k] = static_cast<TO>(xi[j] & mask);
    //         // }
    //       }
    //     });
    //   xassert(histogram_[ndata_chunks_ * nradixes_ - 1] == nrows_);
    // }
};
*/

using sortcolPtr = std::unique_ptr<SorterColumn>;
static sortcolPtr _make_sorter_column(const Column& col) {
  switch (col.stype()) {
    case SType::BOOL: return sortcolPtr(new Boolean_SorterColumn(col));
    default: throw TypeError() << "Cannot sort column of type " << col.stype();
  }
}



}  // namespace dt




namespace py {


static PKArgs args_newsort(0, 0, 0, false, false, {}, "newsort", nullptr);

oobj Frame::newsort(const PKArgs&) {
  xassert(dt->ncols() >= 1);
  xassert(dt->nrows() > 1);

  const Column& col0 = dt->get_column(0);
  auto sorter = dt::_make_sorter_column(col0);
  auto rowindex = sorter->sort();
  Column ricol = rowindex.as_column(dt->nrows());
  return py::Frame::oframe(new DataTable({std::move(ricol)}, {"order"}));
}


void Frame::_init_newsort(XTypeMaker& xt) {
  xt.add(METHOD(&Frame::newsort, args_newsort));
}


}
