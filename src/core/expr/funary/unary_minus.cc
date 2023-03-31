//------------------------------------------------------------------------------
// Copyright 2022-2023 H2O.ai
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
#include "column/func_unary.h"
#include "expr/fexpr_func_unary.h"
#include "python/xargs.h"
#include "stype.h"
namespace dt {
namespace expr {


class FExpr_UnaryMinus : public FExpr_FuncUnary {
  public:
    using FExpr_FuncUnary::FExpr_FuncUnary;


    std::string name() const override {
      return "uminus";
    }

    template <typename T>
    static inline T op_minus(T x) {
      return -x;
    }
    /**
      * Unary operator `+` upcasts each numeric column to INT32, but
      * otherwise keeps unmodified. The operator cannot be applied to
      * string columns.
      */
    Column evaluate1(Column&& col) const override{
      SType stype = col.stype();

      switch (stype) {
        case SType::VOID: 
          return Column(new ConstNa_ColumnImpl(col.nrows(), SType::VOID));
        case SType::BOOL: 
        case SType::INT8: 
        case SType::INT16:
          col.cast_inplace(SType::INT32);
          return Column(new FuncUnary1_ColumnImpl<int32_t, int32_t>(
                   std::move(col), op_minus<int32_t>, col.nrows(), SType::INT32
                 ));
        case SType::INT32:
          return Column(new FuncUnary1_ColumnImpl<int32_t, int32_t>(
                   std::move(col), op_minus<int32_t>, col.nrows(), SType::INT32
                 ));
        case SType::INT64:
          return Column(new FuncUnary1_ColumnImpl<int64_t, int64_t>(
                   std::move(col), op_minus<int64_t>, col.nrows(), SType::INT64
                 ));
        case SType::FLOAT32:
          return Column(new FuncUnary1_ColumnImpl<float, float>(
                   std::move(col), op_minus<float>, col.nrows(), SType::FLOAT32
                 ));
        case SType::FLOAT64:
          return Column(new FuncUnary1_ColumnImpl<double, double>(
                   std::move(col), op_minus<double>, col.nrows(), SType::FLOAT64
                 ));
        default:
          throw TypeError() << "Cannot apply unary `operator -` to a column with "
                              "stype `" << stype << "`";
      }
    }
};

py::oobj PyFExpr::nb__neg__(py::robj src) {
  return PyFExpr::make(
            new FExpr_UnaryMinus(as_fexpr(src)));
}

}}  // dt::expr
