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


template <typename TO, typename TU>
class Sorter_Raw : public Sorter<TO> {
  private:
    using Sorter<TO>::nrows_;
    TU* data_;        // array with nrows_ elements
    Buffer buffer_;   // owner of the data_ pointer
    int n_significant_bits_;
    int : 32;

  public:
  	Sorter_Raw(Buffer&& buf, size_t nrows, int nbits)
  		: Sorter<TO>(nrows),
        data_(static_cast<TU*>(buf.xptr())),
  		  buffer_(std::move(buf)),
        n_significant_bits_(nbits)
  	{
  		xassert(buffer_.size() == nrows * sizeof(TU));
      xassert(nbits > 0 && nbits <= 8 * int(sizeof(TU)));
 	  }

    void sort_subgroup(size_t offset, size_t length,
                       array<TO> ordering_in, array<TO> ordering_out,
                       bool parallel)
    {
      if (length <= INSERTSORT_NROWS) {
        insert_sort(offset, ordering_in, ordering_out);
      } else {
        radix_sort(offset, ordering_in, ordering_out, parallel);
      }
    }

    TU* get_data() const {
      return data_;
    }


  protected:
    int compare_lge(size_t i, size_t j) const override {
      return (data_[i] > data_[j]) - (data_[i] < data_[j]);
    }


    void insert_sort(array<TO> ordering_out) const override {
      insert_sort(0, array<TO>(), ordering_out);
    }

    void radix_sort(array<TO> ordering_out, bool parallel) const override {
      radix_sort(0, array<TO>(), ordering_out, parallel);
    }


  private:
    void insert_sort(size_t offset,
                     array<TO> ordering_in,
                     array<TO> ordering_out) const
    {
      TU* x = data_ + offset;
      dt::sort::small_sort(ordering_in, ordering_out,
        [&](size_t i, size_t j){ return x[i] < x[j]; });
    }


    void radix_sort(size_t offset,
                    array<TO> ordering_in,
                    array<TO> ordering_out,
                    bool parallel) const
    {
      int n_radix_bits = (n_significant_bits_ < 16)? n_significant_bits_ : 8;
      int n_remaining_bits = n_significant_bits_ - n_radix_bits;
      if (n_remaining_bits == 0)       radix_sort0(offset, ordering_in, ordering_out, parallel);
      else if (n_remaining_bits <= 8)  radix_sort1<uint8_t> (offset, ordering_in, ordering_out, n_radix_bits, parallel);
      else if (n_remaining_bits <= 16) radix_sort1<uint16_t>(offset, ordering_in, ordering_out, n_radix_bits, parallel);
      else if (n_remaining_bits <= 32) radix_sort1<uint32_t>(offset, ordering_in, ordering_out, n_radix_bits, parallel);
      else                             radix_sort1<uint64_t>(offset, ordering_in, ordering_out, n_radix_bits, parallel);
    }

    void radix_sort0(size_t offset,
                     array<TO> ordering_in,
                     array<TO> ordering_out,
                     bool parallel) const
    {
      size_t n = ordering_out.size;
      RadixSort rdx(n, n_significant_bits_, parallel);
      TU* x = data_ + offset;
      rdx.sort_by_radix(ordering_in, ordering_out,
        [&](size_t i){ return x[i]; });
    }


    template <typename TNext>
    void radix_sort1(size_t offset,
                     array<TO> ordering_in,
                     array<TO> ordering_out,
                     int n_radix_bits,
                     bool parallel) const
    {
      static_assert(std::is_unsigned<TNext>::value, "Wrong TNext type");
      size_t n = ordering_out.size;
      int shift = n_significant_bits_ - n_radix_bits;
      TU mask = static_cast<TU>(TU(1) << shift) - 1;
      Sorter_Raw<TO, TNext> nextcol(Buffer::mem(n*sizeof(TNext)), n, shift);
      TU* x = data_ + offset;
      TNext* y = nextcol.get_data();

      RadixSort rdx(n, n_radix_bits, parallel);
      array<TO> groups = rdx.sort_by_radix(ordering_in, ordering_out,
        [&](size_t i){ return static_cast<size_t>(x[i] >> shift); },
        [&](size_t i, size_t j){ y[j] = static_cast<TNext>(x[i] & mask); });

      rdx.sort_subgroups(groups, ordering_out, ordering_in,
        [&](size_t offs, size_t length, array<TO> oin, array<TO> oout, bool para) {
          nextcol.sort_subgroup(offs, length, oin, oout, para);
        });
    }
};



}}  // namespace dt::sort
#endif
