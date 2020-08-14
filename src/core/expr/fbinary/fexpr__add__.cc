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
#include "column/func_binary.h"
#include "column/string_plus.h"
#include "expr/fbinary/fexpr_binaryop.h"
namespace dt {
namespace expr {


class FExpr_BinaryPlus : public FExpr_BinaryOp {
  public:
    using FExpr_BinaryOp::FExpr_BinaryOp;
    using FExpr_BinaryOp::lhs_;
    using FExpr_BinaryOp::rhs_;


    std::string name() const override        { return "+"; }
    int precedence() const noexcept override { return 11; }


    Column evaluate1(Column&& lcol, Column&& rcol) const override {
      xassert(lcol.nrows() == rcol.nrows());
      auto stype1 = lcol.stype();
      auto stype2 = rcol.stype();
      auto stype0 = common_stype(stype1, stype2);

      if (stype1 == SType::VOID || stype2 == SType::VOID) {
        return Column::new_na_column(lcol.nrows(), stype0);
      }
      switch (stype0) {
        case SType::BOOL:
        case SType::INT8:
        case SType::INT16:
        case SType::INT32:   return make<int32_t>(std::move(lcol), std::move(rcol), SType::INT32);
        case SType::INT64:   return make<int64_t>(std::move(lcol), std::move(rcol), stype0);
        case SType::FLOAT32: return make<float>(std::move(lcol), std::move(rcol), stype0);
        case SType::FLOAT64: return make<double>(std::move(lcol), std::move(rcol), stype0);
        case SType::STR32:
        case SType::STR64: {
          lcol.cast_inplace(stype0);
          rcol.cast_inplace(stype0);
          return Column(new StringPlus_ColumnImpl(std::move(lcol), std::move(rcol)));
        }
        default:
          throw TypeError() << "Operator `+` cannot be applied to columns of "
            "types `" << stype1 << "` and `" << stype2 << "`";
      }
    }

  private:
    template <typename T>
    static Column make(Column&& a, Column&& b, SType stype) {
      xassert(compatible_type<T>(stype));
      size_t nrows = a.nrows();
      a.cast_inplace(stype);
      b.cast_inplace(stype);
      return Column(new FuncBinary1_ColumnImpl<T, T, T>(
        std::move(a), std::move(b),
        [](T x, T y){ return x + y; },
        nrows, stype
      ));
    }
};



#if 0
static const char* doc__add__ =
R"(__add__(x, y)
--

Add two FExprs together, which corresponds to python operator `+`.

If `x` or `y` are multi-column expressions, then they must have the
same number of columns, and the `+` operator will be applied to each
corresponding pair of columns. If either `x` or `y` are single-column
while the other is multi-column, then the single-column expression
will be repeated to the same number of columns as its opponent.

The result of adding two columns with different stypes will have the
following stype:

  - `max(x.stype, y.stype, int32)` if both columns are numeric (i.e.
    bool, int or float);

  - `str32`/`str64` if at least one of the columns is a string. In
    this case the `+` operator implements string concatenation, same
    as in Python.

Parameters
----------
x, y: FExpr
    The arguments must be either `FExpr`s, or expressions that can be
    converted into `FExpr`s.

return: FExpr
    An expression that evaluates `x + y`.
)";
#endif

py::oobj PyFExpr::nb__add__(py::robj lhs, py::robj rhs) {
  return PyFExpr::make(
            new FExpr_BinaryPlus(as_fexpr(lhs), as_fexpr(rhs)));
}



}}  // namespace dt::expr
