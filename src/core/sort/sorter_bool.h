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
template <typename T, bool ASC>
class Sorter_Bool : public SSorter<T>
{
  using Vec = array<T>;
  using TGrouper = Grouper<T>;
  using UnqSorter = std::unique_ptr<SSorter<T>>;
  using NextWrapper = dt::function<UnqSorter(UnqSorter&&)>;
  private:
    using SSorter<T>::nrows_;
    Column column_;

  public:
    Sorter_Bool(const Column& col)
      : SSorter<T>(col.nrows()),
        column_(col)
    {
      xassert(col.stype() == SType::BOOL);
    }


  protected:
    int compare_lge(size_t i, size_t j) const override {
      int8_t ivalue, jvalue;
      bool ivalid = column_.get_element(i, &ivalue);
      bool jvalid = column_.get_element(j, &jvalue);
      return (ivalid - jvalid) +
             (ivalid & jvalid) * (ASC? ivalue - jvalue : jvalue - ivalue);
    }


    void small_sort(Vec ordering_in, Vec ordering_out, size_t,
                    TGrouper* grouper) const override
    {
      xassert(ordering_in.size() == ordering_out.size());
      const T* oin = ordering_in.ptr();
      dt::sort::small_sort(ordering_in, ordering_out, grouper,
        [&](size_t i, size_t j) {  // compare_lt
          int8_t ivalue, jvalue;
          auto ii = static_cast<size_t>(oin[i]);
          auto jj = static_cast<size_t>(oin[j]);
          bool ivalid = column_.get_element(ii, &ivalue);
          bool jvalid = column_.get_element(jj, &jvalue);
          return jvalid & (!ivalid | (ASC? ivalue < jvalue : ivalue > jvalue));
        });
    }


    void small_sort(Vec ordering_out, TGrouper* grouper) const override {
      dt::sort::small_sort(Vec(), ordering_out, grouper,
        [&](size_t i, size_t j) {  // compare_lt
          int8_t ivalue, jvalue;
          bool ivalid = column_.get_element(i, &ivalue);
          bool jvalid = column_.get_element(j, &jvalue);
          return jvalid & (!ivalid | (ASC? ivalue < jvalue : ivalue > jvalue));
        });
    }


    void radix_sort(Vec ordering_in, Vec ordering_out,
                    size_t offset, TGrouper* grouper, Mode sort_mode,
                    NextWrapper wrap) const override
    {
      (void) offset;
      (void) wrap;
      RadixSort rdx(nrows_, 1, sort_mode);
      auto groups = rdx.sort_by_radix(ordering_in, ordering_out,
          [&](size_t i) {  // get_radix
            int8_t ivalue;
            bool ivalid = column_.get_element(i, &ivalue);
            return static_cast<size_t>(ivalid * (ASC? ivalue + 1 : 2 - ivalue));
          });
      if (grouper) {
        grouper->fill_from_offsets(groups);
      }
    }

};



}}  // namespace dt::sort
#endif
