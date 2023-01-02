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
#include "column/const.h"
#include "column/nth.h"
#include "documentation.h"
#include "expr/fexpr_func.h"
#include "expr/eval_context.h"
#include "python/xargs.h"
#include "expr/fnary/fnary.h"
#include "column/isna.h"
namespace dt {
namespace expr {


class FExpr_Nth : public FExpr_Func {
  private:
    ptrExpr arg_;
    int32_t n_;
    size_t : 32;

  public:
    FExpr_Nth(ptrExpr&& arg, int32_t n)
      : arg_(std::move(arg)),
        n_(n)
        {}

      out += '(';
      out += arg_->repr();
      out += ", n=";
      out += std::to_string(n_);
      for (size_t i = 0; i < inputs.ncols(); ++i) {
        auto coli = inputs.retrieve_column(i);
        outputs.add_column(
          evaluate1(std::move(coli), gby, n_),
          inputs.retrieve_name(i),
        );
      }
      return outputs;
    }


    Column evaluate1(Column&& col, const Groupby& gby, const int32_t n) const {
      SType stype = col.stype();
      switch (stype) {
        case SType::VOID:    return Column(new ConstNa_ColumnImpl(gby.size()));
        case SType::DATE32:
        case SType::INT32:   return make<int32_t>(std::move(col), gby, n);
        case SType::TIME64:
        case SType::INT64:   return make<int64_t>(std::move(col), gby, n);
        case SType::FLOAT32: return make<float>(std::move(col), gby, n);
        case SType::FLOAT64: return make<double>(std::move(col), gby, n);
        case SType::STR32:   return make<CString>(std::move(col), gby, n);
        case SType::STR64:   return make<CString>(std::move(col), gby, n);
        default:
          throw TypeError()
            << "Invalid column of type `" << stype << "` in " << repr();
      }
    }


    template <typename T>
    Column make(Column&& col, const Groupby& gby, int32_t n) const {
      return Column(new Nth_ColumnImpl<T>(std::move(col), gby, n));

static py::oobj pyfn_nth(const py::XArgs& args) {
  auto cols = args[0].to_oobj();
  auto n = args[1].to<int32_t>(0);

  return PyFExpr::make(new FExpr_Nth(as_fexpr(cols), n));


}


}}  // dt::expr