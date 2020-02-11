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
#ifndef dt_SORT_SORTER_INT_h
#define dt_SORT_SORTER_INT_h
#include "sort/insertsort.h"
#include "sort/radixsort.h"
#include "sort/sorter.h"
#include "sort/sorter_uint.h"
#include "column.h"
namespace dt {
namespace sort {


/**
 * Sorter for (virtual) integer columns.
 */
template <typename TO, typename TI>
class Sorter_Int : public Sorter<TO> {
  using TU = std::make_unsigned<TI>::type;
  private:
    Column column_;
    TI min_;

  public:
    Sorter_Int(const Column& col)
      : Sorter<TO>(col.nrows()),
        column_(col) { assert_compatible_type<TI>(col.stype()); }


  protected:
    int compare_lge(size_t i, size_t j) const override {
      TI ivalue, jvalue;
      bool ivalid = column_.get_element(i, &ivalue);
      bool jvalid = column_.get_element(j, &jvalue);
      return ivalid && jvalid? (ivalue > jvalue) - (ivalue < jvalue)
                             : (ivalid - jvalid);
    }

    void insert_sort(array<TO> ordering_out) const override {
      ::insert_sort(ordering_out,
        [&](size_t i, size_t j) -> bool {  // compare_lt
          TI ivalue, jvalue;
          bool ivalid = column_.get_element(i, &ivalue);
          bool jvalid = column_.get_element(j, &jvalue);
          return jvalid && (!ivalid || ivalue < jvalue);
        });
    }

    void radix_sort(array<TO> ordering_out, bool parallel) const override {
      bool minmax_valid;
      int64_t min = column_.stats()->min_int(&minmax_valid);
      int64_t max = column_.stats()->max_int(&minmax_valid);
      if (!minmax_valid) {
        write_range(ordering_out);
        return;
      }

      int nsigbits = sizeof(TI) * 8 - dt::nlz(static_cast<TU>(max - min + 1));
      int nradixbits = std::min(nsigbits, 8);
      int shift = nsigbits - nradixbits;
      size_t mask = (size_t(1) << shift) - 1;

      Buffer tmp = Buffer::mem(sizeof(TO) * nrows_);
      array<TO> ordering_tmp(tmp);

      Buffer out_buffer = Buffer::mem(sizeof(TI) * nrows_);
      array<TI> out_array(out_buffer);

      RadixSort rdx(nrows_, 1, parallel);
      auto groups = rdx.sort_by_radix(ordering_tmp, out_array,
        [&](size_t i, size_t* remainder) -> size_t {  // get_radix
          TI value;
          bool isvalid = column_.get_element(i, &value);
          if (!isvalid) return 0;
          *remainder = static_cast<size_t>(value - min) & mask;
          return 1 + (static_cast<size_t>(value - min) >> shift);
        });

      Sorter_UInt<TO, TU> nextcol(std::move(out_buffer), nrows_);
      rdx.sort_subgroups(groups, ordering_tmp, ordering_out,
        [&](size_t offset, size_t len, array<TO> ord_in, array<TO> ord_out) {
          nextcol.sort(offset, len, ord_in, ord_out);
        });
    }


  private:
    void write_range(array<TO> ordering_out) {
      size_t n = ordering_out.size;
      for (size_t i = 0; i < n; ++i) {
        ordering_out.ptr[i] = static_cast<TO>(i);
      }
    }

};



}}  // namespace dt::sort
#endif
