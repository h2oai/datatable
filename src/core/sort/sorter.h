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
#ifndef dt_SORT_SORTER_h
#define dt_SORT_SORTER_h
#include <utility>           // std::pair
#include "sort/common.h"
#include "sort/grouper.h"    // Grouper
#include "groupby.h"
#include "rowindex.h"
namespace dt {
namespace sort {

template <typename TO> class Sorter_Multi;
using RiGb = std::pair<RowIndex, Groupby>;


/**
  * Virtual class that handles sorting of a column. This class is
  * further subclassed by `SSorter<TO>`.
  */
class Sorter {
  protected:
    size_t nrows_;

    Sorter(size_t n) : nrows_(n) {}

  public:
    virtual ~Sorter() {}

    size_t nrows() const noexcept {
      return nrows_;
    }

    virtual RiGb sort(bool find_groups) const = 0;
};



/**
  * Virtual class that handles sorting of a column. Template parameter
  * <T> corresponds to the type of indices in the resulting rowindex.
  */
template <typename T>
class SSorter : public Sorter
{
  friend class Sorter_Multi<T>;
  using Vec = array<T>;
  using TGrouper = Grouper<T>;
  using UnqGrouper = std::unique_ptr<Grouper<T>>;
  using UnqSorter = std::unique_ptr<SSorter<T>>;
  using NextWrapper = dt::function<UnqSorter(UnqSorter&&)>;

  public:
    SSorter(size_t n) : Sorter(n) {
      xassert(sizeof(T) == 8 || n <= MAX_NROWS_INT32);
    }


    RiGb sort(bool find_groups) const override
    {
      Buffer rowindex_buf = Buffer::mem(nrows_ * sizeof(T));
      Vec ordering_out(rowindex_buf);
      Buffer groups_buf;
      UnqGrouper grouper;
      if (find_groups) {
        groups_buf.resize((nrows_ + 1) * sizeof(T));
        grouper = UnqGrouper(new TGrouper(Vec(groups_buf, 1), 0));
      }

      if (nrows_ <= INSERTSORT_NROWS) {
        small_sort(ordering_out, grouper.get());
      } else {
        radix_sort(Vec(), ordering_out, 0, grouper.get(),
                   Mode::PARALLEL, nullptr);
      }
      auto rowindex_type = sizeof(T) == 4? RowIndex::ARR32 : RowIndex::ARR64;
      RowIndex result_rowindex(std::move(rowindex_buf), rowindex_type);
      Groupby  result_groupby;
      if (find_groups) {
        result_groupby = grouper->to_groupby(std::move(groups_buf));
      }

      return RiGb(std::move(result_rowindex), std::move(result_groupby));
    }


  //----------------------------------------------------------------------------
  // API that should be implemented by the derived classes
  //----------------------------------------------------------------------------
  protected:
    /**
      * Sort the vector of indices `ordering_in` and write the result
      * into `ordering_out`. This method should be single-threaded,
      * and optimized for small `n`s.
      *
      * The sorting is performed according to the values of the
      * underlying column within the range `[offset; offset + n)`.
      *
      * The `grouper` variable may be nullptr. However, if present,
      * the function should store the information about the groups
      * in the data range that was sorted.
      *
      * The recommended way of implementing this methid is via the
      * `dt::sort::small_sort()` function from "sort/insert-sort.h".
      */
    virtual void small_sort(Vec ordering_in, Vec ordering_out,
                            size_t offset, TGrouper* grouper) const = 0;

    /**
      * Same as previous, but `ordering_in` is implicitly equal to
      * `{0, 1, ..., n-1}` and `offset` is 0.
      */
    virtual void small_sort(Vec ordering_out, TGrouper*) const = 0;

    /**
      */
    virtual void radix_sort(Vec ordering_in, Vec ordering_out,
                            size_t offset, TGrouper* grouper,
                            Mode sort_mode, NextWrapper wrap) const = 0;

    /**
      * Comparator function that compares the values of the underlying
      * column at indices `i` and `j`. This function should return:
      *   - a negative value if `val[i] < val[j]`;
      *   - zero if `val[i] == val[j]`;
      *   - a positive value if `val[i] > val[j]`.
      *
      * This function is used by Sorter_MultiImpl<T> only.
      */
    virtual int compare_lge(size_t i, size_t j) const = 0;

    virtual bool contains_reordered_data() const {
      return false;
    }
};



// Factory functions
std::unique_ptr<Sorter> make_sorter(const Column&);
std::unique_ptr<Sorter> make_sorter(const std::vector<Column>&);



}}  // namespace dt::sort
#endif
