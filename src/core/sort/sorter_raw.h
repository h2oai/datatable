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
    TU* data_;        // array with nrows_ elements
    Buffer buffer_;   // owner of the data_ pointer
    int n_significant_bits_;
    int : 32;

  public:
  	Sorter_Raw(Buffer&& buf, size_t nrows, int nbits)
  		: Sorter<TO>(nrows),
        data_(buf.xptr()),
  		  buffer_(std::move(buf)),
        n_significant_bits_(nbits)
  	{
  		xassert(buffer_.size() == nrows * sizeof(TU));
      xassert(nbits > 0 && nbits <= 8 * sizeof(TU));
 	  }

  protected:
    int compare_lge(size_t i, size_t j) const override {
      return (data_[i] > data_[j]) - (data_[i] < data_[j]);
    }


    void insert_sort(array<TO> ordering_out) const override {
      ::insert_sort(ordering_out,
                    [&](size_t i, size_t j){ return data_[i] < data_[j]; });
    }


    void radix_sort(array<TO> ordering_out, bool parallel) const override {
      RadixSort rdx(nrows_, n_radix_bits, parallel);
      auto groups = rdx.sort_by_radix();
    }



};



}}  // namespace dt::sort
#endif
