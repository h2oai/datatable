//------------------------------------------------------------------------------
// Copyright 2022 H2O.ai
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
#ifndef dt_COLUMN_SUM_h
#define dt_COLUMN_SUM_h
#include "column/virtual.h"
#include "stype.h"

namespace dt {

  template <typename T, bool SUM>
  class SumProd_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column col_;
    bool is_grouped_;
    Groupby gby_;

  public:
    SumProd_ColumnImpl(Column &&col, bool is_grouped, const Groupby &gby)
      :Virtual_ColumnImpl(gby.size(), col.stype()),
       col_(std::move(col)),
       is_grouped_(is_grouped),
       gby_(gby)
    {
      xassert(col_.can_be_read_as<T>());
    }


    bool get_element(size_t i, T* out) const override {
      T result = !SUM; 
      T value;     
      size_t i0, i1;
      gby_.get_group(i, &i0, &i1);
      if (is_grouped_){
        bool isvalid = col_.get_element(i, &value);
        if (isvalid){
          if (SUM) {            
            result = static_cast<T>(i1-i0) * value;
          } else {
            size_t power = i1-i0;
            while (power) {
              if (power % 2) {
                result *= value;
              }
              value *= value;
              power /= 2;
            }
          }
        }
      } else {
        for (size_t i = i0; i < i1; ++i) {
          bool isvalid = col_.get_element(i, &value);
          if (isvalid){
            if (SUM) {
              result += value;
            } else {
              result *= value;
            }          
          }
        }
      
      } 
      *out = result;     
      return true; // the result is never a missing value
    }


    ColumnImpl *clone() const override {
      return new SumProd_ColumnImpl(Column(col_), is_grouped_, gby_);
    }


    size_t n_children() const noexcept override {
      return 1;
    }


    const Column &child(size_t i) const override {
      xassert(i == 0);
      (void)i;
      return col_;
    }
  };

}  // namespace dt


#endif
