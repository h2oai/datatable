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
#ifndef dt_NTHSKIPNA_h
#define dt_NTHSKIPNA_h
#include "column/virtual.h"
#include "parallel/api.h"
#include "latent.h"
#include "stype.h"

namespace dt {


template <typename T>
class NthSkipNA_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column col_;
    int32_t nth_;
    Groupby gby_;

  public:
    NthSkipNA_ColumnImpl(Column&& col, int32_t nth,  const Groupby& gby)
      : Virtual_ColumnImpl(gby.size(), col.stype()),
        col_(std::move(col)),
        nth_(nth),        
        gby_(gby)
      {xassert(col_.can_be_read_as<T>());}

    ColumnImpl* clone() const override {
      return new NthSkipNA_ColumnImpl(Column(col_), nth_, gby_);
    }

    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return col_;
    }
    void materialize(Column& col_out, bool) override {
      Latent_ColumnImpl::vivify<T>(col_);

      Column col = Column::new_data_column(gby_.size(), col_.stype());
      auto data = static_cast<T*>(col.get_data_editable());

      auto offsets = gby_.offsets_r();
      dt::parallel_for_dynamic(
        gby_.size(),
        [&](size_t gi) {
          size_t i1 = size_t(offsets[gi]);
          size_t i2 = size_t(offsets[gi + 1]);

          T val;
          size_t n;
          bool is_valid;
          n = nth_<0 ? static_cast<size_t>(nth_)+i2
                     : static_cast<size_t>(nth_)+i1;
          if (n < i1 || n >= i2){
            data[gi] = GETNA<T>();
          } else {
              for (size_t i = n; i < i2; ++i) {
                is_valid = col_.get_element(i, &val);
                if (is_valid) {
                  break;
                }
              }            
            data[gi] = is_valid ? val : GETNA<T>();
          }       
        });

      col_out = std::move(col);
    }


};


}  // namespace dt


#endif
