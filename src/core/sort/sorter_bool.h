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
#include "stype.h"
namespace dt {
namespace sort {


/**
  * Sorter for (virtual) boolean columns.
  */
template <typename T, bool ASC>
class Sorter_VBool : public SSorter<T>
{
  using Vec = array<T>;
  using TGrouper = Grouper<T>;
  using ShrSorter = std::shared_ptr<SSorter<T>>;
  using NextWrapper = dt::function<void(ShrSorter&)>;
  private:
    Column column_;

  public:
    Sorter_VBool(const Column& col)
      : column_(col)
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
      if (ordering_in) {
        const T* oin = ordering_in.start();
        xassert(oin && ordering_in.size() == ordering_out.size());
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
      else {
        dt::sort::small_sort(ordering_in, ordering_out, grouper,
          [&](size_t i, size_t j) {  // compare_lt
            int8_t ivalue, jvalue;
            bool ivalid = column_.get_element(i, &ivalue);
            bool jvalid = column_.get_element(j, &jvalue);
            return jvalid & (!ivalid | (ASC? ivalue < jvalue : ivalue > jvalue));
          });
      }
    }


    void radix_sort(Vec ordering_in, Vec ordering_out,
                    size_t offset, TGrouper* grouper, Mode sort_mode,
                    NextWrapper replace_sorter) const override
    {
      xassert(offset == 0);  (void) offset;

      constexpr int nradixbits = 1;
      ShrSorter next_sorter = nullptr;
      if (replace_sorter) {
        replace_sorter(next_sorter);
      }

      RadixSort rdx(ordering_out.size(), nradixbits, sort_mode);
      rdx.sort(ordering_in, ordering_out, next_sorter.get(), grouper,
          [&](size_t i) {  // get_radix
            int8_t ivalue;
            bool ivalid = column_.get_element(i, &ivalue);
            return static_cast<size_t>(ivalid * (ASC? ivalue + 1 : 2 - ivalue));
          },
          [](size_t, size_t) {}  // move_data
      );
    }

};



/**
  * Sorter for a boolean (material) column.
  */
template <typename T, bool ASC>
class Sorter_MBool : public SSorter<T>
{
  using Vec = array<T>;
  using TGrouper = Grouper<T>;
  using ShrSorter = std::shared_ptr<SSorter<T>>;
  using NextWrapper = dt::function<void(ShrSorter&)>;
  private:
    const int8_t* data_;

  public:
    Sorter_MBool(const Column& col)
    {
      xassert(ASC);
      xassert(col.get_na_storage_method() == NaStorage::SENTINEL);
      data_ = static_cast<const int8_t*>(col.get_data_readonly());
    }

  protected:
    int compare_lge(size_t i, size_t j) const override {
      int8_t xi = data_[i];
      int8_t xj = data_[j];
      return ASC? static_cast<int>(xi) - static_cast<int>(xj)
                : static_cast<int>(static_cast<uint8_t>(xj)) -
                  static_cast<int>(static_cast<uint8_t>(xi));
    }


    void small_sort(Vec ordering_in, Vec ordering_out, size_t,
                    TGrouper* grouper) const override
    {
      if (ordering_in) {
        const T* oin = ordering_in.start();
        xassert(oin && ordering_in.size() == ordering_out.size());
        dt::sort::small_sort(ordering_in, ordering_out, grouper,
            [&](size_t i, size_t j) {
              return data_[oin[i]] < data_[oin[j]];
            });
      }
      else {
        dt::sort::small_sort(Vec(), ordering_out, grouper,
            [&](size_t i, size_t j) {
              return data_[i] < data_[j];
            });
      }
    }


    void radix_sort(Vec ordering_in, Vec ordering_out,
                    size_t, TGrouper* grouper, Mode sort_mode,
                    NextWrapper replace_sorter) const override
    {
      constexpr int nradixbits = 1;
      ShrSorter next_sorter = nullptr;
      if (replace_sorter) {
        replace_sorter(next_sorter);
      }

      RadixSort rdx(ordering_out.size(), nradixbits, sort_mode);
      rdx.sort(ordering_in, ordering_out, next_sorter.get(), grouper,
          [&](size_t i) {  // get_radix
            int8_t ivalue = data_[i];
            return ISNA<int8_t>(ivalue)? 0 : static_cast<size_t>(ivalue + 1);
          },
          [](size_t, size_t) {}  // move_data
      );
    }

};




//------------------------------------------------------------------------------
// Factory function
//------------------------------------------------------------------------------

template <typename T, bool ASC>
std::unique_ptr<SSorter<T>> make_sorter_bool(const Column& column) {
  if (ASC && !column.is_virtual()) {
    return std::unique_ptr<SSorter<T>>(new Sorter_MBool<T, ASC>(column));
  } else {
    return std::unique_ptr<SSorter<T>>(new Sorter_VBool<T, ASC>(column));
  }
}




}}  // namespace dt::sort
#endif
