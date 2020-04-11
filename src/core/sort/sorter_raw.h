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
#ifndef dt_SORT_SORTER_RAW_h
#define dt_SORT_SORTER_RAW_h
#include "sort/common.h"
#include "sort/insert-sort.h"
#include "sort/radix-sort.h"
#include "sort/sorter.h"
namespace dt {
namespace sort {


/**
  * Sorter for "raw" (i.e. unsigned integer) data. This type of data
  * is most directly suitable for radix sorting, since its bits can
  * be used to construct radixes directly.
  *
  * This "raw" data does not exist in datatable as-is, however most
  * other data types can be converted into this representation through
  * a simple transform.
  *
  * <T>:  the type of elements in the ordering vector
  * <TU>: the type of elements in the underlying data vector
  *
  */
template <typename T, typename TU>
class Sorter_Raw : public SSorter<T>
{
  using Vec = array<T>;
  using TGrouper = Grouper<T>;
  using ShrSorter = std::shared_ptr<SSorter<T>>;
  using NextWrapper = dt::function<void(ShrSorter&)>;

  private:
    TU* data_;        // array with nrows_ elements
    Buffer buffer_;   // owner of the data_ pointer
    int n_significant_bits_;
    int : 32;

  public:
    Sorter_Raw(Buffer&& buf, size_t nrows, int nbits)
      : data_(static_cast<TU*>(buf.xptr())),
        buffer_(std::move(buf)),
        n_significant_bits_(nbits)
    {
      (void) nrows;
      xassert(buffer_.size() == nrows * sizeof(TU));
      xassert(nbits > 0 && nbits <= 8 * int(sizeof(TU)));
    }


    TU* get_data() const noexcept {
      return data_;
    }


  protected:
    int compare_lge(size_t i, size_t j) const override {
      TU xi = data_[i];
      TU xj = data_[j];
      return (xi > xj) - (xi < xj);
    }

    bool contains_reordered_data() const override {
      return true;
    }

    void small_sort(Vec ordering_in, Vec ordering_out,
                    size_t offset, TGrouper* grouper) const override
    {
      const TU* x = data_ + offset;
      dt::sort::small_sort(ordering_in, ordering_out, grouper,
        [&](size_t i, size_t j){ return x[i] < x[j]; });
    }


    void radix_sort(Vec ordering_in, Vec ordering_out, size_t offset,
                    TGrouper* grouper, Mode mode, NextWrapper replace_sorter
                    ) const override
    {
      int n_radix_bits = (n_significant_bits_ < 16)? n_significant_bits_ : 8;
      int n_remaining_bits = n_significant_bits_ - n_radix_bits;
      if (n_remaining_bits == 0)       radix_sort0(ordering_in, ordering_out, offset, grouper, mode, replace_sorter);
      else if (n_remaining_bits <= 8)  radix_sort1<uint8_t> (ordering_in, ordering_out, offset, n_radix_bits, mode);
      else if (n_remaining_bits <= 16) radix_sort1<uint16_t>(ordering_in, ordering_out, offset, n_radix_bits, mode);
      else if (n_remaining_bits <= 32) radix_sort1<uint32_t>(ordering_in, ordering_out, offset, n_radix_bits, mode);
      else                             radix_sort1<uint64_t>(ordering_in, ordering_out, offset, n_radix_bits, mode);
    }


  private:
    void radix_sort0(Vec ordering_in, Vec ordering_out, size_t offset,
                    TGrouper* grouper, Mode mode, NextWrapper replace_sorter) const
    {
      ShrSorter next_sorter = nullptr;
      if (replace_sorter) {
        replace_sorter(next_sorter);
      }

      size_t n = ordering_out.size();
      TU* x = data_ + offset;
      RadixSort rdx(n, n_significant_bits_, mode);
      rdx.sort(ordering_in, ordering_out, next_sorter.get(), grouper,
        [&](size_t i){ return x[i]; },
        [](size_t, size_t){});
    }


    template <typename TNext>
    void radix_sort1(Vec ordering_in, Vec ordering_out, size_t offset,
                     int n_radix_bits, Mode mode) const
    {
      static_assert(std::is_unsigned<TNext>::value, "Wrong TNext type");
      size_t n = ordering_out.size();
      int shift = n_significant_bits_ - n_radix_bits;
      TU mask = static_cast<TU>(TU(1) << shift) - 1;
      Sorter_Raw<T, TNext> nextcol(Buffer::mem(n*sizeof(TNext)), n, shift);
      TU* x = data_ + offset;
      TNext* y = nextcol.get_data();

      Buffer tmp_buffer = Buffer::mem(n * sizeof(T));
      Vec ordering_tmp(tmp_buffer);

      RadixSort rdx(n, n_radix_bits, mode);
      Vec groups = rdx.sort_by_radix(ordering_in, ordering_tmp,
        [&](size_t i){ return static_cast<size_t>(x[i] >> shift); },
        [&](size_t i, size_t j){ y[j] = static_cast<TNext>(x[i] & mask); });

      rdx.sort_subgroups(groups, ordering_tmp, ordering_out, &nextcol);
    }
};



}}  // namespace dt::sort
#endif
