//------------------------------------------------------------------------------
// Copyright 2020-2021 H2O.ai
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
#include <type_traits>          // std::is_same
#include "sort/insert-sort.h"   // dt::sort::insert_sort
#include "sort/radix-sort.h"    // RadixSort
#include "sort/sorter.h"        // Sort
#include "sort/sorter_raw.h"    // Sorter_Raw
#include "utils/misc.h"         // dt::nlz
#include "column.h"
#include "stype.h"
namespace dt {
namespace sort {


/**
 * Sorter for (virtual) integer columns.
 *
 * <T>   : type of elements in the ordering vector;
 * <TI>  : type of elements in the underlying integer column.
 * <ASC> : sort ascending (if true), or descending (if false)
 */
template <typename T, bool ASC, typename TI>
class Sorter_Int : public SSorter<T>
{
  static_assert(std::is_same<TI, int8_t>::value ||
                std::is_same<TI, int16_t>::value ||
                std::is_same<TI, int32_t>::value ||
                std::is_same<TI, int64_t>::value, "Wrong TI in Sorter_Int");
  using TU = typename std::make_unsigned<TI>::type;
  using Vec = array<T>;
  using TGrouper = Grouper<T>;
  using ShrSorter = std::shared_ptr<SSorter<T>>;
  using NextWrapper = dt::function<void(ShrSorter&)>;

  private:
    Column column_;

  public:
    Sorter_Int(const Column& col)
      : column_(col)
    {
      xassert(col.can_be_read_as<TI>());
    }


  protected:
    int compare_lge(size_t i, size_t j) const override {
      TI ivalue, jvalue;
      bool ivalid = column_.get_element(i, &ivalue);
      bool jvalid = column_.get_element(j, &jvalue);
      return ivalid && jvalid? (ASC? (ivalue > jvalue) - (ivalue < jvalue)
                                   : (ivalue < jvalue) - (ivalue > jvalue))
                             : (ivalid - jvalid);
    }

    void small_sort(Vec ordering_in, Vec ordering_out, size_t,
                    TGrouper* grouper) const override
    {
      if (ordering_in) {
        const T* oin = ordering_in.start();
        xassert(oin && ordering_in.size() == ordering_out.size());
        dt::sort::small_sort(ordering_in, ordering_out, grouper,
          [&](size_t i, size_t j) -> bool {  // compare_lt
            TI ivalue, jvalue;
            auto ii = static_cast<size_t>(oin[i]);
            auto jj = static_cast<size_t>(oin[j]);
            bool ivalid = column_.get_element(ii, &ivalue);
            bool jvalid = column_.get_element(jj, &jvalue);
            return jvalid && (!ivalid || (ASC? ivalue < jvalue
                                             : ivalue > jvalue));
          });
      }
      else {
        dt::sort::small_sort(Vec(), ordering_out, grouper,
          [&](size_t i, size_t j) -> bool {  // compare_lt
            TI ivalue, jvalue;
            bool ivalid = column_.get_element(i, &ivalue);
            bool jvalid = column_.get_element(j, &jvalue);
            return jvalid && (!ivalid || (ASC? ivalue < jvalue
                                             : ivalue > jvalue));
          });
      }
    }


    void radix_sort(Vec ordering_in, Vec ordering_out, size_t,
                    TGrouper* grouper, Mode sort_mode,
                    NextWrapper replace_sorter) const override
    {
      xassert(!ordering_in || ordering_in.size() == ordering_out.size());
      size_t n = ordering_out.size();

      // Computing min/max of a column also calculates the nacount stat;
      // but not the other way around. Therefore, `nacount` must be retrieved
      // after `min` / `max`.
      // The validity flags on min/max are disregarded, because min/max are
      // invalid iff nacount==nrows.
      TI min = static_cast<TI>(column_.stats()->min_int());
      TI max = static_cast<TI>(column_.stats()->max_int());
      size_t nacount = column_.stats()->nacount();

      // If either all values are NAs, or all values are same and there are no
      // NAs, then there is no need to sort.
      if (nacount == n || (min == max && nacount == 0)) {
        write_range(ordering_out);
        return;
      }

      const int nsigbits = dt::nsb(static_cast<TU>(max - min));
      const int nradixbits = std::min(nsigbits, 8);
      const int shift = nsigbits - nradixbits;
      const TU mask = static_cast<TU>((size_t(1) << shift) - 1);

      ShrSorter next_sorter = nullptr;
      array<TU> out_array;
      if (shift) {
        auto rawptr = new Sorter_Raw<T, TU>(Buffer::mem(sizeof(TU) * n),
                                            n, shift);
        out_array = array<TU>(rawptr->get_data(), n);
        next_sorter = ShrSorter(rawptr);
      }
      if (replace_sorter) {
        replace_sorter(next_sorter);
      }

      RadixSort rdx(n, nradixbits, sort_mode);
      if (shift) {
        rdx.sort(ordering_in, ordering_out, next_sorter.get(), grouper,
          [&](size_t i) -> size_t {  // get_radix
            TI value;
            bool isvalid = column_.get_element(i, &value);
            auto uvalue = static_cast<size_t>(ASC? value - min : max - value);
            return isvalid? 1 + (uvalue >> shift) : 0;
          },
          [&](size_t i, size_t j) {  // move_data
            TI value;
            column_.get_element(i, &value);
            auto uvalue = static_cast<TU>(ASC? value - min : max - value);
            out_array[j] = uvalue & mask;
          }
        );
      }
      else {
        rdx.sort(ordering_in, ordering_out, next_sorter.get(), grouper,
          [&](size_t i) -> size_t {  // get_radix
            TI value;
            bool isvalid = column_.get_element(i, &value);
            auto uvalue = static_cast<size_t>(ASC? value - min : max - value);
            return isvalid? 1 + (uvalue >> shift) : 0;
          },
          [](size_t, size_t){});
      }
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
