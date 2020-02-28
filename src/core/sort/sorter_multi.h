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
  using NextWrapper = dt::function<UnqSorter(UnqSorter&&)>;
  private:
    ShrSorter column0_;
    std::vector<ShrSorter> other_columns_;

  public:
    Sorter_Multi(std::vector<UnqSorter>&& cols)
      : SSorter<T>(cols[0]->nrows()),
        column0_(std::move(cols[0]))
    {
      other_columns_.reserve(cols.size() - 1);
      for (size_t i = 1; i < cols.size(); ++i) {
        other_columns_.push_back(ShrSorter(std::move(cols[i])));
      }
    }

    Sorter_Multi(UnqSorter&& col0, const std::vector<ShrSorter>& cols1)
      : SSorter<T>(col0->nrows()),
        column0_(std::move(col0)),
        other_columns_(cols1)
    {}

  protected:
    void small_sort(Vec ordering_out, TGrouper* grouper) const override {
      dt::sort::small_sort(Vec(), ordering_out, grouper,
        [&](size_t i, size_t j) {  // compare_lt
          int cmp = column0_->compare_lge(i, j);
          if (cmp == 0) {
            for (size_t k = 0; k < other_columns_.size(); ++k) {
              cmp = other_columns_[k]->compare_lge(i, j);
              if (cmp) break;
            }
          }
          return (cmp < 0);
        });
    }

    void small_sort(Vec ordering_in, Vec ordering_out, size_t offset,
                    TGrouper* grouper) const override
    {
      xassert(ordering_in.size() == ordering_out.size());
      if (column0_->contains_reordered_data()) {
        dt::sort::small_sort(ordering_in, ordering_out, grouper,
          [&](size_t i, size_t j) {
            int cmp = column0_->compare_lge(i + offset, j + offset);
            if (cmp == 0) {
              size_t ii = static_cast<size_t>(ordering_in[i]);
              size_t jj = static_cast<size_t>(ordering_in[j]);
              for (size_t k = 0; k < other_columns_.size(); ++k) {
                cmp = other_columns_[k]->compare_lge(ii, jj);
                if (cmp) break;
              }
            }
            return (cmp < 0);
          });
      }
      else {
        dt::sort::small_sort(ordering_in, ordering_out, grouper,
          [&](size_t i, size_t j) {
            size_t ii = static_cast<size_t>(ordering_in[i]);
            size_t jj = static_cast<size_t>(ordering_in[j]);
            int cmp = column0_->compare_lge(ii, jj);
            if (cmp == 0) {
              for (size_t k = 0; k < other_columns_.size(); ++k) {
                cmp = other_columns_[k]->compare_lge(ii, jj);
                if (cmp) break;
              }
            }
            return (cmp < 0);
          });
      }
    }

    void radix_sort(Vec ordering_in, Vec ordering_out, size_t offset,
                    TGrouper* grouper, Mode sort_mode, NextWrapper wrap
                    ) const override
    {
      (void) wrap;
      column0_->radix_sort(ordering_in, ordering_out, offset, grouper, sort_mode,
          [&](UnqSorter&& next_sorter) {
            if (next_sorter) {
              return UnqSorter(new Sorter_Multi<T>(std::move(next_sorter),
                                                   other_columns_));
            } else {
              return UnqSorter();  // FIXME
              // return ssoptr(new Sorter_MultiImpl<T>());
            }
          });
    }

    int compare_lge(size_t, size_t) const override {
      throw RuntimeError() << "Sorter_Multi cannot be nested";
    }

};





}} // namespace dt::sort
#endif
