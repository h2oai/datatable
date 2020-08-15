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
#include "expr/fbinary/fexpr_binaryop.h"
namespace dt {
namespace expr {


template <typename T>
inline static bool op_modulo(T x, bool xvalid, T y, bool yvalid, T* out) {
  if (!xvalid || !yvalid || y == 0) return false;
  T res = x % y;
  if ((x < 0) != (y < 0) && res != 0) {
    res += y;
  }
  *out = res;
  return true;
}



class FExpr__mod__ : public FExpr_BinaryOp {
  public:
    using FExpr_BinaryOp::FExpr_BinaryOp;
    using FExpr_BinaryOp::lhs_;
    using FExpr_BinaryOp::rhs_;


    std::string name() const override        { return "%"; }
    int precedence() const noexcept override { return 12; }


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
        case SType::INT32: return make<int32_t>(std::move(lcol), std::move(rcol), SType::INT32);
        case SType::INT64: return make<int64_t>(std::move(lcol), std::move(rcol), SType::INT64);
        default:
          throw TypeError() << "Operator `%` cannot be applied to columns of "
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
      return Column(new FuncBinary2_ColumnImpl<T, T, T>(
                          std::move(a), std::move(b), op_modulo, nrows, stype)
                    );
    }
};




py::oobj PyFExpr::nb__mod__(py::robj lhs, py::robj rhs) {
  return PyFExpr::make(
            new FExpr__mod__(as_fexpr(lhs), as_fexpr(rhs)));
}



}}  // namespace dt::expr
