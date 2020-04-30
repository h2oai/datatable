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
#include <type_traits>       // std::is_same
#include <utility>           // std::pair
#include "sort/common.h"
#include "sort/grouper.h"    // Grouper
#include "groupby.h"
#include "rowindex.h"
namespace dt {
namespace sort {

template <typename TO> class Sorter_Multi;


/**
  * Virtual class that handles sorting of a column. This class is
  * further subclassed by `SSorter<TO>`.
  */
class Sorter
{
  public:
    virtual ~Sorter() {}

    virtual RiGb sort(size_t n, bool find_groups) const = 0;
};



/**
  * Virtual class that handles sorting of a column. Template parameter
  * <T> corresponds to the type of indices in the resulting rowindex.
  */
template <typename T>
class SSorter : public Sorter
{
  static_assert(std::is_same<T, int32_t>::value ||
                std::is_same<T, int64_t>::value, "Wrong SSorter<T>");
  friend class Sorter_Multi<T>;
  using Vec = array<T>;
  using TGrouper = Grouper<T>;
  using UnqGrouper = std::unique_ptr<Grouper<T>>;
  using ShrSorter = std::shared_ptr<SSorter<T>>;
  using NextWrapper = dt::function<void(ShrSorter&)>;

  public:
    RiGb sort(size_t n, bool find_groups) const override
    {
      xassert(sizeof(T) == 8 || n <= MAX_NROWS_INT32);
      Buffer rowindex_buf = Buffer::mem(n * sizeof(T));
      Vec ordering_out(rowindex_buf);
      xassert(ordering_out.size() == n);

      Buffer groups_buf;
      UnqGrouper grouper;
      if (find_groups) {
        groups_buf.resize((n + 1) * sizeof(T));
        grouper = UnqGrouper(new TGrouper(Vec(groups_buf, 1), 0));
      }

      sort(Vec(), ordering_out, 0, grouper.get(), Mode::PARALLEL);

      auto rowindex_type = sizeof(T) == 4? RowIndex::ARR32 : RowIndex::ARR64;
      RowIndex result_rowindex(std::move(rowindex_buf), rowindex_type);
      Groupby  result_groupby;
      if (find_groups) {
        result_groupby = grouper->to_groupby(std::move(groups_buf));
      }

      xassert(result_rowindex.max() == n - 1);
      return RiGb(std::move(result_rowindex), std::move(result_groupby));
    }


    void sort(Vec ordering_in, Vec ordering_out,
              size_t offset, TGrouper* grouper, Mode sort_mode) const
    {
      size_t n = ordering_out.size();
      if (n <= INSERTSORT_NROWS) {
        small_sort(ordering_in, ordering_out, offset, grouper);
      } else {
        radix_sort(ordering_in, ordering_out, offset, grouper,
                   sort_mode, nullptr);
      }
      #if DT_DEBUG
        check_sorted(ordering_out);
      #endif
    }


  //----------------------------------------------------------------------------
  // API that should be implemented by the derived classes
  //----------------------------------------------------------------------------
  protected:
    /**
      * Sort the vector of indices `ordering_in` and write the result
      * into `ordering_out`. This method should be single-threaded,
      * and optimized for small `n`s. The vector `ordering_in` can
      * also be empty, in which it should be treated as if it was
      * equal to `{0, 1, ..., n-1}`.
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
      * This function is mainly used by Sorter_Multi<T>.
      */
    virtual int compare_lge(size_t i, size_t j) const = 0;

    virtual bool contains_reordered_data() const {
      return false;
    }


  private:
    void check_sorted(Vec ordering) const {
      #if DT_DEBUG
        // The `ordering` vector corresponds to the ordering of the
        // original data vector. If the Sorter contains reordered
        // data, then this ordering is not valid for it.
        if (contains_reordered_data()) return;

        size_t n = ordering.size();
        if (!n) return;
        size_t j0 = static_cast<size_t>(ordering[0]);
        for (size_t i = 1; i < n; ++i) {
          size_t j1 = static_cast<size_t>(ordering[i]);
          int r = compare_lge(j0, j1);
          xassert(r <= 0);
          j0 = j1;
        }
      #else
        (void) ordering;
      #endif
    }
};



// Factory functions
std::unique_ptr<Sorter> make_sorter(const Column&, Direction dir);
std::unique_ptr<Sorter> make_sorter(const std::vector<Column>&);



}}  // namespace dt::sort
#endif
