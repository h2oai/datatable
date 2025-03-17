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
inline static T op_lshift(T x, int32_t y) {
  return (y >= 0)? static_cast<T>(x << y) : static_cast<T>(x >> -y);
}

template <typename T>
inline static T op_rshift(T x, int32_t y) {
  return (y >= 0)? static_cast<T>(x >> y) : static_cast<T>(x << -y);
}


template<bool LSHIFT>
class FExprBitShift : public FExpr_BinaryOp {
  public:
    using FExpr_BinaryOp::FExpr_BinaryOp;
    using FExpr_BinaryOp::lhs_;
    using FExpr_BinaryOp::rhs_;


    std::string name() const override        { return LSHIFT?"<<":">>"; }
    int precedence() const noexcept override { return 10; }


    Column evaluate1(Column&& lcol, Column&& rcol) const override {
      xassert(lcol.nrows() == rcol.nrows());
      auto stype1 = lcol.stype();
      auto stype2 = rcol.stype();
      xassert(Type::from_stype(stype1).is_integer());
      xassert(Type::from_stype(stype1).is_integer() || Type::from_stype(stype1).is_boolean()); 

      auto stype0 = (stype2 == SType::INT32)? SType::AUTO : SType::INT32;      

      switch (stype1) {
        case SType::INT8:    return make<int8_t, LSHIFT>(std::move(lcol), std::move(rcol), stype0);
        case SType::INT16:   return make<int16_t, LSHIFT>(std::move(lcol), std::move(rcol), stype0);
        case SType::INT32:   return make<int32_t, LSHIFT>(std::move(lcol), std::move(rcol), stype0);
        case SType::INT64:   return make<int64_t, LSHIFT>(std::move(lcol), std::move(rcol), stype0);
        default:
          std::string op = LSHIFT?"<<":">>";
          throw TypeError() << "Operator " << op << " cannot be applied to columns of "
            "types `" << stype1 << "` and `" << stype2 << "`";
      }
    }

  private:
    template <typename T, bool L_SHIFT>
    static Column make(Column&& a, Column&& b, SType stype) {
      xassert(compatible_type<T>(a.stype()));
      size_t nrows = a.nrows();
      b.cast_inplace(stype);
      if (L_SHIFT) {
        return Column(new FuncBinary1_ColumnImpl<T, int32_t, T>(
          std::move(a), std::move(b),
          op_lshift<T>,
          nrows, a.stype()
        ));
      }
      return Column(new FuncBinary1_ColumnImpl<T, int32_t, T>(
        std::move(a), std::move(b),
        op_rshift<T>,
        nrows, a.stype()
      ));
    }
};


py::oobj PyFExpr::nb__lshift__(py::robj lhs, py::robj rhs) {
  return PyFExpr::make(
            new FExprBitShift<true>(as_fexpr(lhs), as_fexpr(rhs)));
}

py::oobj PyFExpr::nb__rshift__(py::robj lhs, py::robj rhs) {
  return PyFExpr::make(
            new FExprBitShift<false>(as_fexpr(lhs), as_fexpr(rhs)));
}

}}  // namespace dt::expr




