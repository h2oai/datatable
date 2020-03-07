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
#ifndef dt_SORT_SORTER_FLOAT_h
#define dt_SORT_SORTER_FLOAT_h
#include <type_traits>          // std::is_same
#include "sort/insert-sort.h"   // dt::sort::insert_sort
#include "sort/radix-sort.h"    // RadixSort
#include "sort/sorter.h"        // Sort
#include "sort/sorter_raw.h"    // Sorter_Raw
#include "column.h"
namespace dt {
namespace sort {


template <typename T> struct FloatConstants {};
template <> struct FloatConstants<float> {
  using utype = uint32_t;
  static constexpr utype EXP = 0x7F800000;
  static constexpr utype MNT = 0x007FFFFF;
  static constexpr utype SBT = 0x80000000;
};
template <> struct FloatConstants<double> {
  using utype = uint64_t;
  static constexpr utype EXP = 0x7FF0000000000000ULL;
  static constexpr utype MNT = 0x000FFFFFFFFFFFFFULL;
  static constexpr utype SBT = 0x8000000000000000ULL;
};



/**
 * Sorter for (virtual) float columns.
 *
 * <T>   : type of elements in the ordering vector;
 * <TE>  : type of elements in the underlying float column.
 * <ASC> : sort ascending (if true), or descending (if false)
 */
template <typename T, bool ASC, typename TE>
class Sorter_Float : public SSorter<T>
{
  static_assert(std::is_same<TE, float>::value ||
                std::is_same<TE, double>::value, "Wrong TE in Sorter_Float");
  using TU = typename FloatConstants<TE>::utype;
  static_assert(sizeof(TE) == sizeof(TU), "Wrong TU in Sorter_Float");
  using Vec = array<T>;
  using TGrouper = Grouper<T>;
  using UnqSorter = std::unique_ptr<SSorter<T>>;
  using NextWrapper = dt::function<void(UnqSorter&)>;

  private:
    using SSorter<T>::nrows_;
    Column column_;

    static constexpr TU EXP = FloatConstants<TE>::EXP;
    static constexpr TU MNT = FloatConstants<TE>::MNT;
    static constexpr TU SBT = FloatConstants<TE>::SBT;
    static constexpr int SHIFT = sizeof(TU) * 8 - 1;
    static_assert(SBT == TU(1)<<SHIFT, "bad SHIFT/SBT");

  public:
    Sorter_Float(const Column& col)
      : SSorter<T>(col.nrows()),
        column_(col)
    {
      assert_compatible_type<TE>(col.stype());
    }


  protected:
    int compare_lge(size_t i, size_t j) const override {
      TE ivalue, jvalue;
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
        const T* oin = ordering_in.ptr();
        xassert(oin && ordering_in.size() == ordering_out.size());
        dt::sort::small_sort(ordering_in, ordering_out, grouper,
          [&](size_t i, size_t j) -> bool {  // compare_lt
            TE ivalue, jvalue;
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
            TE ivalue, jvalue;
            bool ivalid = column_.get_element(i, &ivalue);
            bool jvalid = column_.get_element(j, &jvalue);
            return jvalid && (!ivalid || (ASC? ivalue < jvalue
                                             : ivalue > jvalue));
          });
      }
    }


    void radix_sort(Vec ordering_in, Vec ordering_out, size_t offset,
                    TGrouper* grouper, Mode sort_mode, NextWrapper wrap
                    ) const override
    {
      (void) grouper;
      (void) wrap;
      xassert(!ordering_in);   (void) ordering_in;
      xassert(!offset);        (void) offset;

      constexpr int nsigbits = 8 * sizeof(TE);
      constexpr int nradixbits = 8;
      constexpr int shift = nsigbits - nradixbits;
      constexpr TU mask = static_cast<TU>((size_t(1) << shift) - 1);

      Buffer tmp_buffer = Buffer::mem(sizeof(T) * nrows_);
      Buffer out_buffer = Buffer::mem(sizeof(TU) * nrows_);
      Vec ordering_tmp(tmp_buffer);
      array<TU> out_array(out_buffer);

      RadixSort rdx(nrows_, nradixbits, sort_mode);
      auto groups = rdx.sort_by_radix(Vec(), ordering_tmp,
        [&](size_t i) -> size_t {  // get_radix
          TU value;
          bool isvalid = column_.get_element(i, reinterpret_cast<TE*>(&value));
          value = ((value & EXP) == EXP && (value & MNT) != 0) ? 0 :
                    ASC? value ^ (SBT | -(value>>SHIFT))
                       : value ^ (~SBT & ((value>>SHIFT) - 1));
          return isvalid? 1 + (value >> shift) : 0;
        },
        [&](size_t i, size_t j) {  // move_data
          TU value;
          column_.get_element(i, reinterpret_cast<TE*>(&value));
          value = ((value & EXP) == EXP && (value & MNT) != 0) ? 0 :
                    ASC? value ^ (SBT | -(value>>SHIFT))
                       : value ^ (~SBT & ((value>>SHIFT) - 1));
          out_array[j] = value & mask;
        });

      Sorter_Raw<T, TU> nextcol(std::move(out_buffer), nrows_, shift);
      rdx.sort_subgroups(groups, ordering_tmp, ordering_out, &nextcol);
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
