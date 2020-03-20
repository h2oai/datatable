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
#ifndef dt_SORT_SORTER_MULTI_h
#define dt_SORT_SORTER_MULTI_h
#include <memory>                // std::shared_ptr
#include <vector>                // std::vector
#include "sort/common.h"         // array
#include "sort/insert-sort.h"    // small_sort
#include "sort/sorter.h"         // SSorter
namespace dt {
namespace sort {



template <typename T>
class Sorter_Multi : public SSorter<T>
{
  using Vec = array<T>;
  using TGrouper = Grouper<T>;
  using UnqSorter = std::unique_ptr<SSorter<T>>;
  using ShrSorter = std::shared_ptr<SSorter<T>>;
  using NextWrapper = dt::function<void(ShrSorter&)>;
  using SorterVec = std::vector<ShrSorter>;

  private:
    SorterVec columns_;

  public:
    Sorter_Multi(std::vector<UnqSorter>&& cols)
    {
      xassert(cols.size() > 1);
      columns_.reserve(cols.size());
      for (auto& col : cols) {
        columns_.push_back(ShrSorter(std::move(col)));
      }
    }

    Sorter_Multi(ShrSorter&& col0, const SorterVec& cols1)
    {
      columns_.reserve(1 + cols1.size());
      columns_.push_back(std::move(col0));
      for (const auto& col : cols1) {
        columns_.push_back(col);
      }
    }

    Sorter_Multi(SorterVec&& cols)
      : columns_(std::move(cols))
    {
      xassert(cols.size() > 1);
    }


  protected:
    void small_sort(Vec ordering_in, Vec ordering_out, size_t offset,
                    TGrouper* grouper) const override
    {
      if (!ordering_in) {
        dt::sort::small_sort(Vec(), ordering_out, grouper,
          [&](size_t i, size_t j) {  // compare_lt
            for (const auto& col : columns_) {
              int cmp = col->compare_lge(i, j);
              if (cmp) return (cmp < 0);
            }
            return false;
          });
      }
      else if (columns_[0]->contains_reordered_data()) {
        xassert(ordering_in.size() == ordering_out.size());
        dt::sort::small_sort(ordering_in, ordering_out, grouper,
          [&](size_t i, size_t j) {
            int cmp = columns_[0]->compare_lge(i + offset, j + offset);
            if (cmp == 0) {
              size_t ii = static_cast<size_t>(ordering_in[i]);
              size_t jj = static_cast<size_t>(ordering_in[j]);
              for (size_t k = 1; k < columns_.size(); ++k) {
                cmp = columns_[k]->compare_lge(ii, jj);
                if (cmp) break;
              }
            }
            return (cmp < 0);
          });
      }
      else {
        xassert(ordering_in.size() == ordering_out.size());
        dt::sort::small_sort(ordering_in, ordering_out, grouper,
          [&](size_t i, size_t j) {
            size_t ii = static_cast<size_t>(ordering_in[i]);
            size_t jj = static_cast<size_t>(ordering_in[j]);
            for (const auto& col : columns_) {
              int cmp = col->compare_lge(ii, jj);
              if (cmp) return (cmp < 0);
            }
            return false;
          });
      }
    }


    void radix_sort(Vec ordering_in, Vec ordering_out, size_t offset,
                    TGrouper* grouper, Mode sort_mode, NextWrapper wrap
                    ) const override
    {
      xassert(!wrap);  (void) wrap;
      columns_[0]->radix_sort(
          ordering_in, ordering_out, offset, grouper, sort_mode,
          [&](ShrSorter& next_sorter) {
            SorterVec remaining_columns(columns_.begin() + 1,
                                        columns_.end());
            if (next_sorter) {
              next_sorter = ShrSorter(
                  new Sorter_Multi<T>(std::move(next_sorter),
                                      std::move(remaining_columns))
              );
            } else if (remaining_columns.size() > 1) {
              next_sorter = ShrSorter(
                  new Sorter_Multi<T>(std::move(remaining_columns))
              );
            } else {
              next_sorter = std::move(remaining_columns[0]);
            }
          });
    }

    // may be called from `check_sorted()`
    int compare_lge(size_t i, size_t j) const override {
      int cmp = 0;
      for (const auto& col : columns_) {
        cmp = col->compare_lge(i, j);
        if (cmp) break;
      }
      return cmp;
    }

};





}} // namespace dt::sort
#endif
