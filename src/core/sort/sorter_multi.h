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

template <typename TO>
class Sorter_MultiImpl;


template <typename TO>
class Sorter_Multi : public SSorter<TO> {
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
                    size_t offset, bool parallel) const override
    {
      impl_.radix_sort(ordering_in, ordering_out, offset, parallel);
    }

    int compare_lge(size_t i, size_t j) const override {
      return impl_.compare_lge(i, j);
    }
};



template <typename TO>
class Sorter_MultiImpl : public SSorter<TO>
{
  friend class Sorter_Multi<TO>;
  private:
    using ovec = array<TO>;
    using SSorter<TO>::nrows_;
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
      (void) ordering_out;
      throw NotImplError();
    }

    void small_sort(ovec ordering_in, ovec ordering_out, size_t offset)
        const override
    {
      (void) ordering_in;
      (void) ordering_out;
      (void) offset;
      throw NotImplError();
    }

    virtual void radix_sort(ovec ordering_in, ovec ordering_out,
                            size_t offset, bool parallel) const override
    {
      (void) ordering_in;
      (void) ordering_out;
      (void) offset;
      (void) parallel;
      throw NotImplError();
    }

    int compare_lge(size_t i, size_t j) const override {
      (void) i;
      (void) j;
      throw NotImplError();
    }
    // void small_sort(ovec ordering_out) const override {
    //   if (column0_.contains_reordered_data())
    //     _small_sort_impl<true>(ordering_out);
    //   else
    //     _small_sort_impl<false>(ordering_out);
    // }

    // template <bool Col0Reordered>
    // void _small_sort_impl(ovec ordering_out) const {
    //   dt::sort::small_sort(ovec(), ordering_out,
    //     [&](size_t i, size_t j) {  // compare_lt
    //       int cmp =
    //       int8_t ivalue, jvalue;
    //       bool ivalid = column_.get_element(i, &ivalue);
    //       bool jvalid = column_.get_element(j, &jvalue);
    //       return jvalid && (!ivalid || ivalue < jvalue);
    //     });
    // }

};





}} // namespace dt::sort
#endif
