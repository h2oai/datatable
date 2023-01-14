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
#ifndef dt_COLUMN_MINMAX_h
#define dt_COLUMN_MINMAX_h
#include "column/virtual.h"
#include "stype.h"
namespace dt {


template <typename T, bool MIN>
class MinMax_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column col_;
    Groupby gby_;
    bool is_grouped_;
    size_t : 56;

  public:
    MinMax_ColumnImpl(Column &&col, const Groupby& gby, bool is_grouped)
      : Virtual_ColumnImpl(gby.size(), col.stype()),
        col_(std::move(col)),
        gby_(gby),
        is_grouped_(is_grouped)
    {
      xassert(col_.can_be_read_as<T>());
    }


    bool get_element(size_t i, T* out) const override {      
      size_t i0, i1;
      gby_.get_group(i, &i0, &i1);

      if (is_grouped_) {
        T value;
        bool is_valid = col_.get_element(i, &value);
        *out = value;
        return is_valid;
      } else {
          T minmax = MIN ? std::numeric_limits<T>::min()
                         : std::numeric_limits<T>::max();
          bool isna = true;
          for (size_t gi = i0; gi < i1; ++gi){
            T value;
            bool isvalid = col_.get_element(gi, &value);
            if (isvalid && MIN) {
              if (value < minmax || isna){
                minmax = value;
                isna = false;
              }
            } else if (isvalid && !MIN) {
                if (value > minmax || isna){
                  minmax = value;
                  isna = false;
                }
              }
          }
        *out = static_cast<T>(minmax);
        return !isna;             
        }
         
      
    }


    ColumnImpl *clone() const override {
      return new MinMax_ColumnImpl(Column(col_), Groupby(gby_), is_grouped_);
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
