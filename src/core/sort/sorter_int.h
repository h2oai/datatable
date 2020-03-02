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
#include "sort/insert-sort.h"   // dt::sort::insert_sort
#include "sort/radix-sort.h"    // RadixSort
#include "sort/sorter.h"        // Sort
#include "sort/sorter_raw.h"    // Sorter_Raw
#include "utils/misc.h"         // dt::nlz
#include "column.h"
namespace dt {
namespace sort {


/**
 * SSorter for (virtual) integer columns.
 */
template <typename T, typename TI>
class Sorter_Int : public SSorter<T> {
  using TU = typename std::make_unsigned<TI>::type;
  using Vec = array<T>;
  using TGrouper = Grouper<T>;
  using UnqSorter = std::unique_ptr<SSorter<T>>;
  using NextWrapper = dt::function<UnqSorter(UnqSorter&&)>;
  private:
    using SSorter<T>::nrows_;
    Column column_;

  public:
    Sorter_Int(const Column& col)
      : SSorter<T>(col.nrows()),
        column_(col) { assert_compatible_type<TI>(col.stype()); }


  protected:
    int compare_lge(size_t i, size_t j) const override {
      TI ivalue, jvalue;
      bool ivalid = column_.get_element(i, &ivalue);
      bool jvalid = column_.get_element(j, &jvalue);
      return ivalid && jvalid? (ivalue > jvalue) - (ivalue < jvalue)
                             : (ivalid - jvalid);
    }

    void small_sort(Vec ordering_in, Vec ordering_out,
                    size_t offset, TGrouper* grouper) const override
    {
      (void) offset;
      xassert(ordering_in.size() == ordering_out.size());
      const T* oin = ordering_in.ptr();
      dt::sort::small_sort(ordering_in, ordering_out, grouper,
        [&](size_t i, size_t j) -> bool {  // compare_lt
          TI ivalue, jvalue;
          auto ii = static_cast<size_t>(oin[i]);
          auto jj = static_cast<size_t>(oin[j]);
          bool ivalid = column_.get_element(ii, &ivalue);
          bool jvalid = column_.get_element(jj, &jvalue);
          return jvalid && (!ivalid || ivalue < jvalue);
        });
    }

    void small_sort(Vec ordering_out, TGrouper* grouper) const override {
      dt::sort::small_sort(Vec(), ordering_out, grouper,
        [&](size_t i, size_t j) -> bool {  // compare_lt
          TI ivalue, jvalue;
          bool ivalid = column_.get_element(i, &ivalue);
          bool jvalid = column_.get_element(j, &jvalue);
          return jvalid && (!ivalid || ivalue < jvalue);
        });
    }

    void radix_sort(Vec ordering_in, Vec ordering_out, size_t offset,
                    TGrouper* grouper, Mode sort_mode, NextWrapper wrap
                    ) const override
    {
      (void) grouper;
      (void) wrap;
      (void) offset;
      (void) ordering_in;
      xassert(ordering_in.size() == 0);
      xassert(offset == 0);
      bool minmax_valid;
      TI min = static_cast<TI>(column_.stats()->min_int(&minmax_valid));
      TI max = static_cast<TI>(column_.stats()->max_int(&minmax_valid));
      if (!minmax_valid) {
        write_range(ordering_out);
        return;
      }

      int nsigbits = static_cast<int>(sizeof(TI) * 8) -
                      dt::nlz(static_cast<TU>(max - min + 1));
      int nradixbits = std::min(nsigbits, 8);
      int shift = nsigbits - nradixbits;
      TU mask = static_cast<TU>((size_t(1) << shift) - 1);

      Buffer tmp = Buffer::mem(sizeof(T) * nrows_);
      Vec ordering_tmp(tmp);

      Buffer out_buffer = Buffer::mem(sizeof(TU) * nrows_);
      array<TU> out_array(out_buffer);

      RadixSort rdx(nrows_, 1, sort_mode);
      auto groups = rdx.sort_by_radix(Vec(), ordering_tmp,
        [&](size_t i) -> size_t {  // get_radix
          TI value;
          bool isvalid = column_.get_element(i, &value);
          return isvalid? 1 + (static_cast<size_t>(value - min) >> shift) : 0;
        },
        [&](size_t i, size_t j) {  // move_data
          TI value;
          column_.get_element(i, &value);
          out_array[j] = static_cast<TU>(value - min) & mask;
        });

      Sorter_Raw<T, TU> nextcol(std::move(out_buffer), nrows_, shift);
      rdx.sort_subgroups(groups, ordering_tmp, ordering_out,
        [&](size_t offs, size_t len, Vec ord_in, Vec ord_out, Mode m) {
          nextcol.sort_subgroup(offs, len, ord_in, ord_out, nullptr, m);
        });
    }


  private:
    void write_range(Vec ordering_out) const {
      size_t n = ordering_out.size();
      for (size_t i = 0; i < n; ++i) {
        ordering_out[i] = static_cast<T>(i);
      }
    }

};



}}  // namespace dt::sort
#endif
