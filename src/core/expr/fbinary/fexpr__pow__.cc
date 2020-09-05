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
inline T op_power(T x, T y) {
  return static_cast<T>(std::pow(static_cast<double>(x),
                                 static_cast<double>(y)));
}

template <>
inline float op_power(float x, float y) {
  return std::pow(x, y);
}



class FExpr__pow__ : public FExpr_BinaryOp {
  public:
    using FExpr_BinaryOp::FExpr_BinaryOp;
    using FExpr_BinaryOp::lhs_;
    using FExpr_BinaryOp::rhs_;


    std::string name() const override        { return "**"; }
    int precedence() const noexcept override { return 14; }

    // Operator ** is right-associative, so repr() must take this into
    // account when setting up parentheses around the arguments. For
    // clarity's sake, we surround both sides with parentheses if they
    // are at the same precedence as this operator.
    std::string repr() const override {
      auto lstr = lhs_->repr();
      auto rstr = rhs_->repr();
      if (lhs_->precedence() <= precedence()) {
        lstr = "(" + lstr + ")";
      }
      if (rhs_->precedence() <= precedence()) {
        rstr = "(" + rstr + ")";
      }
      return lstr + " ** " + rstr;
    }


    /**
      * Operator `**` implements the following rules:
      *
      *   VOID ** {*} -> VOID
      *   {BOOL, INT8, INT16, INT32} ** {same} -> INT32
      *   INT64 ** {BOOL, INT*} -> INT64
      *   FLOAT32 ** {BOOL, INT*, FLOAT32} -> FLOAT32
      *   FLOAT64 ** {BOOL, INT*, FLOAT*} -> FLOAT64
      *
      *
      */
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
        case SType::INT32:
        case SType::INT64:
        case SType::FLOAT64: return make<double>(std::move(lcol), std::move(rcol), SType::FLOAT64);
        case SType::FLOAT32: return make<float>(std::move(lcol), std::move(rcol), stype0);
        default:
          throw TypeError() << "Operator `**` cannot be applied to columns of "
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
        std::move(a), std::move(b), op_power<T>, nrows, stype
      ));
    }
};



py::oobj PyFExpr::nb__pow__(py::robj lhs, py::robj rhs, py::robj zhs) {
  if (zhs && !zhs.is_none()) {
    throw NotImplError() << "2-argument form of pow() is not supported";
  }
  // Under normal rules, an int raised to an integer power produces
  // an integer. This may lead to surprising results such as
  //  `2 ** -2 == 0` (while `2 ** -2.0 == 0.25`).
  // For this reason, when the user writes `expr ** a` and `a` is a
  // simple integer that is negative, then we convert that integer
  // into a float. Thus, `f.A ** -1` will produce a float64 column
  // even if column "A" is int32.
  py::oobj power = rhs;
  if (rhs.is_int()) {
    auto x = rhs.to_int64();
    if (x < 0) {
      power = py::ofloat(static_cast<double>(x));
    }
  }
  return PyFExpr::make(
            new FExpr__pow__(as_fexpr(lhs), as_fexpr(power)));
}



}}  // namespace dt::expr
