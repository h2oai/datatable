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
 * SSorter for (virtual) boolean columns.
 */
template <typename TO>
class Sorter_Bool : public SSorter<TO> {
  private:
    using ovec = array<TO>;
    using typename SSorter<TO>::next_wrapper;
    using SSorter<TO>::nrows_;
    Column column_;

  public:
    Sorter_Bool(const Column& col)
      : SSorter<TO>(col.nrows()),
        column_(col) { xassert(col.stype() == SType::BOOL); }


  protected:
    int compare_lge(size_t i, size_t j) const override {
      int8_t ivalue, jvalue;
      bool ivalid = column_.get_element(i, &ivalue);
      bool jvalid = column_.get_element(j, &jvalue);
      return (ivalid - jvalid) + (ivalid & jvalid) * (ivalue - jvalue);
    }


    void small_sort(ovec ordering_in, ovec ordering_out, size_t) const override
    {
      xassert(ordering_in.size == ordering_out.size);
      const TO* oin = ordering_in.ptr;
      dt::sort::small_sort(ordering_in, ordering_out,
        [&](size_t i, size_t j) {  // compare_lt
          int8_t ivalue, jvalue;
          bool ivalid = column_.get_element(static_cast<size_t>(oin[i]), &ivalue);
          bool jvalid = column_.get_element(static_cast<size_t>(oin[j]), &jvalue);
          return jvalid && (!ivalid || ivalue < jvalue);
        });
    }


    void small_sort(ovec ordering_out) const override {
      dt::sort::small_sort(ovec(), ordering_out,
        [&](size_t i, size_t j) {  // compare_lt
          int8_t ivalue, jvalue;
          bool ivalid = column_.get_element(i, &ivalue);
          bool jvalid = column_.get_element(j, &jvalue);
          return jvalid && (!ivalid || ivalue < jvalue);
        });
    }


    void radix_sort(ovec ordering_in, ovec ordering_out,
                    size_t offset, bool parallel,
                    next_wrapper wrap = nullptr) const override
    {
      (void) offset;
      RadixSort rdx(nrows_, 1, parallel);
      rdx.sort_by_radix(ordering_in, ordering_out,
        [&](size_t i) {  // get_radix
          int8_t ivalue;
          bool ivalid = column_.get_element(i, &ivalue);
          return static_cast<size_t>(ivalid * (ivalue + 1));
        });
    }

};



}}  // namespace dt::sort
#endif
