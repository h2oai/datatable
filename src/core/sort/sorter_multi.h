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
#include "sort/sorter.h"         // Sorter
namespace dt {
namespace sort {


template <typename TO>
class Sorter_MultiImpl : public Sorter<TO> {
  private:
    using ovec = array<TO>;
    using Sorter<TO>::nrows_;
    Sorter<TO>* column0_;
    array<Sorter<TO>*> other_columns_;

  public:
    Sorter_MultiImpl(Sorter<TO>* col0, array<Sorter<TO>*> cols)
      : column0_(col0),
        other_columns_(cols) {}

  protected:
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



template <typename TO>
class Sorter_Multi : public Sorter<TO> {
  using ovec = array<TO>;
  private:
    std::vector<std::unique_ptr<Sorter<TO>>> sorters_;
    Sorter_MultiImpl<TO> impl_;

  public:
    Sorter_Multi(std::vector<std::unique_ptr<Sorter<TO>>>&& cols)
      : Sorter<TO>(cols[0]->nrows()),
        sorters_(std::move(cols)),
        impl_(static_cast<Sorter<TO>**>(sorters_.data()),
              sorters_.size()) {}

  protected:
    void small_sort(ovec ordering_out) const override {
      impl_.small_sort(ordering_out);
    }
};




}} // namespace dt::sort
#endif
