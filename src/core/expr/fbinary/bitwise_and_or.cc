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
static bool op_and_bool(ref_t<T> x, bool xvalid, ref_t<T> y, bool yvalid, int8_t* out)
{
  if (x == 0 && xvalid) {  // short-circuit
    *out = 0;
    return true;
  }
  if (!yvalid) return false;
  if (y == 0) {
    *out = 0;
    return true;
  }
  *out = 1;
  return xvalid;
}


template <typename T>
static bool op_or_bool(ref_t<T> x, bool xvalid, ref_t<T> y, bool yvalid, int8_t* out)
{
  if (x == 1 && xvalid) {  // short-circuit
    *out = 1;
    return true;
  }
  if (!yvalid) return false;
  if (y == 1) {
    *out = 1;
    return true;
  }
  *out = 0;
  return xvalid;
}


template <typename T>
inline static T op_and(T x, T y) {
  return (x & y);
}

template <typename T>
inline static T op_or(T x, T y) {
  return (x | y);
}

template <typename T>
inline static T op_xor(T x, T y) {
  return (x ^ y);
}

template<bool AND>
class FExpr__andor__ : public FExpr_BinaryOp {
  public:
    using FExpr_BinaryOp::FExpr_BinaryOp;
    using FExpr_BinaryOp::lhs_;
    using FExpr_BinaryOp::rhs_;


    std::string name() const override        { return AND?"&":"|"; }
    int precedence() const noexcept override { return AND?4:3; }


    Column evaluate1(Column&& lcol, Column&& rcol) const override {
      xassert(lcol.nrows() == rcol.nrows());
      size_t nrows = lcol.nrows();
      auto stype1 = lcol.stype();
      auto stype2 = rcol.stype();
      auto stype0 = common_stype(stype1, stype2);

      if (stype1 == SType::VOID || stype2 == SType::VOID) {
        return Column::new_na_column(lcol.nrows(), SType::VOID);
      }
      if (stype1 == SType::BOOL && stype2 == SType::BOOL) {
        if (AND) {
          return Column(new FuncBinary2_ColumnImpl<int8_t, int8_t, int8_t>(
            std::move(lcol), std::move(rcol),
            op_and_bool<int8_t>,
            nrows, SType::BOOL
          ));          
        }
        return Column(new FuncBinary2_ColumnImpl<int8_t, int8_t, int8_t>(
          std::move(lcol), std::move(rcol),
          op_or_bool<int8_t>,
          nrows, SType::BOOL
        ));
      }
      switch (stype0) {
        case SType::INT8:    return make<int8_t, AND>(std::move(lcol), std::move(rcol), stype0);
        case SType::INT16:   return make<int16_t, AND>(std::move(lcol), std::move(rcol), stype0);
        case SType::INT32:   return make<int32_t, AND>(std::move(lcol), std::move(rcol), stype0);
        case SType::INT64:   return make<int64_t, AND>(std::move(lcol), std::move(rcol), stype0);
        default:
          char op = AND?'&':'|';
          throw TypeError() << "Operator " << op << " cannot be applied to columns of "
            "types `" << stype1 << "` and `" << stype2 << "`";
      }
    }

  private:
    template <typename T, bool ANDD>
    static Column make(Column&& a, Column&& b, SType stype) {
      xassert(compatible_type<T>(stype));
      size_t nrows = a.nrows();
      a.cast_inplace(stype);
      b.cast_inplace(stype);
      if (AND) {
        return Column(new FuncBinary1_ColumnImpl<T, T, T>(
          std::move(a), std::move(b),
          op_and<T>,
          nrows, stype
        ));
      }
      return Column(new FuncBinary1_ColumnImpl<T, T, T>(
        std::move(a), std::move(b),
        op_or<T>,
        nrows, stype
      ));
    }
};


class FExpr__xor__ : public FExpr_BinaryOp {
  public:
    using FExpr_BinaryOp::FExpr_BinaryOp;
    using FExpr_BinaryOp::lhs_;
    using FExpr_BinaryOp::rhs_;


    std::string name() const override        { return "^"; }
    int precedence() const noexcept override { return 8; }


    Column evaluate1(Column&& lcol, Column&& rcol) const override {
      xassert(lcol.nrows() == rcol.nrows());
      auto stype1 = lcol.stype();
      auto stype2 = rcol.stype();
      auto stype0 = common_stype(stype1, stype2);

      switch (stype0) {
        case SType::BOOL:    return make<int8_t>(std::move(lcol), std::move(rcol), stype0);
        case SType::INT8:    return make<int8_t>(std::move(lcol), std::move(rcol), stype0);
        case SType::INT16:   return make<int16_t>(std::move(lcol), std::move(rcol), stype0);
        case SType::INT32:   return make<int32_t>(std::move(lcol), std::move(rcol), stype0);
        case SType::INT64:   return make<int64_t>(std::move(lcol), std::move(rcol), stype0);
        default:
          throw TypeError() << "Operator `^` cannot be applied to columns of "
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
        op_xor<T>,
        nrows, stype
      ));
    }
};


py::oobj PyFExpr::nb__and__(py::robj lhs, py::robj rhs) {
  return PyFExpr::make(
            new FExpr__andor__<true>(as_fexpr(lhs), as_fexpr(rhs)));
}

py::oobj PyFExpr::nb__or__(py::robj lhs, py::robj rhs) {
  return PyFExpr::make(
            new FExpr__andor__<false>(as_fexpr(lhs), as_fexpr(rhs)));
}

py::oobj PyFExpr::nb__xor__(py::robj lhs, py::robj rhs) {
  return PyFExpr::make(
            new FExpr__xor__(as_fexpr(lhs), as_fexpr(rhs)));
}



}}  // namespace dt::expr


