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
#include <algorithm>           // std::stable_sort
#include <tuple>               // std::make_tuple, std::tie
#include "frame/py_frame.h"    // py::Frame
#include "parallel/api.h"      // parallel_for_static
#include "python/args.h"       // py::PKArgs
#include "sort/common.h"       // array
#include "sort/insertsort.h"   // insert_sort
#include "sort/radixsort.h"    // RadixSort
#include "utils/assert.h"      // xassert
#include "utils/misc.h"        // dt::nlz
#include "buffer.h"            // Buffer
#include "column.h"            // Column
#include "rowindex.h"          // RowIndex
namespace dt {
namespace sort {



static size_t nrows_small = 16;
// static size_t nrows_quicksort = 256;


/**
 * Class that wraps a column in the process of being sorted.
 */
class SorterColumn {

  //--------------------------------------------------------------------------
  // Public interface
  //--------------------------------------------------------------------------
  public:
    SorterColumn() = default;
    virtual ~SorterColumn() = default;


    /**
      * Main sorting function
      */
    RowIndex sort() {
      size_t nrows = get_nrows();
      bool use_int32 = (nrows <= size_t(1u << 31));
      size_t elemsize = use_int32? sizeof(int32_t) : sizeof(int64_t);
      Buffer rowindex_buf = Buffer::mem(nrows * elemsize);

      if (nrows < nrows_small) {
        xassert(use_int32);
        sort_small(array<int32_t>(rowindex_buf));
      }
      else if (use_int32) {
        radix_sort(array<int32_t>(rowindex_buf), true);
      }
      else {
        radix_sort(array<int64_t>(rowindex_buf), true);
      }
      return RowIndex(std::move(rowindex_buf),
                      use_int32? RowIndex::ARR32 : RowIndex::ARR64);
    }


  private:
    template <typename TO>
    void radix_sort(array<TO> ordering_out, bool allow_parallel)
    {
      RadixSort rdx(get_nrows(), get_n_radix_bits(), allow_parallel);
      array<TO> group_offsets = rdx.sort_by_radix(ordering_out, n);

      // if (has_more_radix_bits()) {
      //   size_t* group_offsets = histogram + (n_chunks_ - 1) * nradixes_;
      //   const size_t large_group = std::max(n / 4, 100000);

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
      // }
    }


    template <typename TO>
    void reorder_indices(TO* indices, size_t n, TO* order) {
      dt::parallel_for_static(n,
        [&](size_t i) {
          order[i] = indices[order[i]];
        });
      dt::parallel_for_static(n,
        [&](size_t i) {
          indices[i] = order[i];
        })
    }


  protected:
    /**
      * Return the number of rows in the column.
      */
    virtual size_t get_nrows() const = 0;

    /**
      * Sort the entire column using either the insert-sort or the
      * std::sort algorithm, and write the result into the provided
      * `ordering_out` array.
      */
    virtual void sort_small(array<int32_t> ordering_out) const = 0;

    template <typename TO>
    virtual void sort_small(size_t offset,
                            array<TO> ordering_in,
                            array<TO> ordering_out) const = 0;

    /**
      * Return the number of bits this column should use for the radix
      * sort algorithm. The return value can be less than the total
      * number of bits available, in which case radix-sort will sort
      * the values only partially.
      *
      * The returned value will be used to allocate an array of
      * `nthreads * (1 << get_n_radix_bits())` elements, which means
      * you shouldn't try to return values larger than 20 (and even
      * this is probably too much).
      */
    virtual int get_n_radix_bits() = 0;


    /**
      * Return `false` if the bits returned by `get_radix_bits()` are
      * sufficient to sort the values completely, or true if they
      * sort only partially and more radix bits are available.
      */
    virtual bool has_more_radix_bits() = 0;


    /**
      * Return the radix of the value at row `i`. The returned value
      * must be less than or equal to `1 << get_n_radix_bits()`.
      */
    virtual size_t get_radix(size_t i) const = 0;

};




class Boolean_SorterColumn : public SorterColumn {
  private:
    Column column_;

  public:
    Boolean_SorterColumn(const Column& col)
      : column_(col) { xassert(col.stype() == SType::BOOL); }

  protected:
    size_t get_nrows() const override {
      return column_.nrows();
    }

    void sort_small(array<int32_t> ordering_out) const override {
      dt::sort::simple_sort(ordering_out.ptr, get_nrows(),
        [&](size_t i, size_t j) {
          int8_t ivalue, jvalue;
          bool ivalid = column_.get_element(i, &ivalue);
          bool jvalid = column_.get_element(j, &jvalue);
          return jvalid && (!ivalid || ivalue < jvalue);
        });
    }

    template <typename TO>
    void sort_small(size_t offset,
                    array<TO> ordering_in,
                    array<TO> ordering_out) const override
    {
      dt::sort::simple_sort(ordering_in, ordering_out,
        [&](size_t i, size_t j) {
          int8_t ivalue, jvalue;
          bool ivalid = column_.get_element(i + offset, &ivalue);
          bool jvalid = column_.get_element(j + offset, &jvalue);
          return jvalid && (!ivalid || ivalue < jvalue);
        });
    }


    int get_n_radix_bits() override {
      return 1;
    }

    bool has_more_radix_bits() override {
      return false;
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

    int get_n_radix_bits() override {
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



using sortcolPtr = std::unique_ptr<SorterColumn>;
static sortcolPtr _make_sorter_column(const Column& col) {
  switch (col.stype()) {
    case SType::BOOL: return sortcolPtr(new Boolean_SorterColumn(col));
    default: throw TypeError() << "Cannot sort column of type " << col.stype();
  }
}



}}  // namespace dt::sort




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
