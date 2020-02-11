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
#ifndef dt_SORT_SORTER_BOOL_h
#define dt_SORT_SORTER_BOOL_h
#include "sort/insert-sort.h"
#include "sort/sorter.h"
#include "column.h"
namespace dt {
namespace sort {


/**
 * Sorter for (virtual) boolean columns.
 */
template <typename TO>
class Sorter_Bool : public Sorter<TO> {
  private:
    Column column_;

  public:
    Sorter_Bool(const Column& col)
      : Sorter<TO>(col.nrows()),
        column_(col) { xassert(col.stype() == SType::BOOL); }


  protected:
    int compare_lge(size_t i, size_t j) const override {
      int8_t ivalue, jvalue;
      bool ivalid = column_.get_element(i, &ivalue);
      bool jvalid = column_.get_element(j, &jvalue);
      return (ivalid - jvalid) + (ivalid & jvalid) * (ivalue - jvalue);
    }

    void insert_sort(array<TO> ordering_out) const override {
      ::insert_sort(ordering_out,
        [&](size_t i, size_t j) {  // compare_lt
          int8_t ivalue, jvalue;
          bool ivalid = column_.get_element(i, &ivalue);
          bool jvalid = column_.get_element(j, &jvalue);
          return jvalid && (!ivalid || ivalue < jvalue);
        });
    }

    void radix_sort(array<TO> ordering_out, bool parallel) const override {
      RadixSort rdx(nrows_, 1, parallel);
      rdx.sort_by_radix(ordering_out,
        [&](size_t i) {  // get_radix
          int8_t ivalue;
          bool ivalid = column_.get_element(i, &ivalue);
          return static_cast<size_t>(isvalid * (value + 1));
        });
    }

};



}}  // namespace dt::sort
#endif
