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
#include <vector>                // std::vector
#include "sort/common.h"         // array
#include "sort/insert-sort.h"    // small_sort
#include "sort/sorter.h"         // SSorter
namespace dt {
namespace sort {

// forward-declare
template <typename TO> class Sorter_MultiImpl;


template <typename TO>
class Sorter_Multi : public SSorter<TO>
{
  using typename SSorter<TO>::next_wrapper;
  using ptrSsorter = std::unique_ptr<SSorter<TO>>;
  using ovec = array<TO>;
  private:
    std::vector<ptrSsorter> sorters_;
    Sorter_MultiImpl<TO> impl_;

  public:
    Sorter_Multi(std::vector<ptrSsorter>&& cols)
      : SSorter<TO>(cols[0]->nrows()),
        sorters_(std::move(cols)),
        impl_(reinterpret_cast<SSorter<TO>**>(sorters_.data()),
              sorters_.size()) {}

  protected:
    void small_sort(ovec ordering_out) const override {
      impl_.small_sort(ordering_out);
    }

    void small_sort(ovec ordering_in, ovec ordering_out,
                    size_t offset) const override
    {
      impl_.small_sort(ordering_in, ordering_out, offset);
    }

    void radix_sort(ovec ordering_in, ovec ordering_out,
                    size_t offset, bool parallel,
                    next_wrapper wrap = nullptr) const override
    {
      impl_.radix_sort(ordering_in, ordering_out, offset, parallel, wrap);
    }

    int compare_lge(size_t i, size_t j) const override {
      return impl_.compare_lge(i, j);
    }
};



template <typename TO>
class Sorter_MultiImpl : public SSorter<TO>
{
  friend class Sorter_Multi<TO>;
  using typename SSorter<TO>::ovec;
  using typename SSorter<TO>::ssoptr;
  using typename SSorter<TO>::next_wrapper;
  using Sorter::nrows_;
  private:
    SSorter<TO>* column0_;
    array<SSorter<TO>*> other_columns_;

  public:
    Sorter_MultiImpl(SSorter<TO>* col0, array<SSorter<TO>*> cols)
      : SSorter<TO>(col0->nrows()),
        column0_(col0),
        other_columns_(cols) {}

    Sorter_MultiImpl(SSorter<TO>** cols, size_t n)
      : SSorter<TO>(cols[0]->nrows()),
        column0_(cols[0]),
        other_columns_(cols + 1, n - 1) {}

  protected:
    void small_sort(ovec ordering_out) const override {
      dt::sort::small_sort(ovec(), ordering_out,
        [&](size_t i, size_t j) {  // compare_lt
          int cmp = column0_->compare_lge(i, j);
          if (cmp == 0) {
            for (size_t k = 0; k < other_columns_.size; ++k) {
              cmp = other_columns_.ptr[k]->compare_lge(i, j);
              if (cmp) break;
            }
          }
          return (cmp < 0);
        });
    }

    void small_sort(ovec ordering_in, ovec ordering_out, size_t offset)
        const override
    {
      xassert(ordering_in.size == ordering_out.size);
      if (column0_->contains_reordered_data()) {
        dt::sort::small_sort(ordering_in, ordering_out,
          [&](size_t i, size_t j) {
            int cmp = column0_->compare_lge(i + offset, j + offset);
            if (cmp == 0) {
              size_t ii = static_cast<size_t>(ordering_in.ptr[i]);
              size_t jj = static_cast<size_t>(ordering_in.ptr[j]);
              for (size_t k = 0; k < other_columns_.size; ++k) {
                cmp = other_columns_.ptr[k]->compare_lge(ii, jj);
                if (cmp) break;
              }
            }
            return (cmp < 0);
          });
      }
      else {
        dt::sort::small_sort(ordering_in, ordering_out,
          [&](size_t i, size_t j) {
            size_t ii = static_cast<size_t>(ordering_in.ptr[i]);
            size_t jj = static_cast<size_t>(ordering_in.ptr[j]);
            int cmp = column0_->compare_lge(ii, jj);
            if (cmp == 0) {
              for (size_t k = 0; k < other_columns_.size; ++k) {
                cmp = other_columns_.ptr[k]->compare_lge(ii, jj);
                if (cmp) break;
              }
            }
            return (cmp < 0);
          });
      }
    }

    virtual void radix_sort(ovec ordering_in, ovec ordering_out,
                            size_t offset, bool parallel,
                            next_wrapper wrap = nullptr) const override
    {
      column0_->radix_sort(ordering_in, ordering_out, offset, parallel,
          [&](const ssoptr& next_sorter) {
            if (next_sorter) {
              return ssoptr(new Sorter_MultiImpl<TO>(next_sorter.get(),
                                                     other_columns_));
            } else {
              return ssoptr();  // FIXME
              // return ssoptr(new Sorter_MultiImpl<TO>());
            }
          });
    }

    int compare_lge(size_t, size_t) const override {
      throw RuntimeError() << "Sorter_Multi cannot be nested";
    }

};





}} // namespace dt::sort
#endif
