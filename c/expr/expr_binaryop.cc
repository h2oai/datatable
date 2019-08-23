//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
// This file implements binary operations between columbs, such as Plus, Minus,
// Multiply, etc. That is, if `x` and `y` are two columns of compatible
// dimensions, then the function `expr::binaryop` will compute the column which
// is a result of simple arithmethic expression such as
//     x + y
//     x - y
//     x * y   etc.
//
// This sounds trivial, however it most certainly is not. There are many
// possible combinations that need to be considered, depending on the stypes of
// `x` and `y` (which could be different), on the `op`, and on the type of
// column compatibility (n-to-n, n-to-1, or 1-to-n). We require that columns be
// reified already (i.e. have no rowindices), otherwise it would have required
// 16 (or 25) times more code...
//
// In order to help tame this explosion of possibilities, templates are used
// heavily in this source file.
//------------------------------------------------------------------------------
#include <cmath>               // std::fmod
#include <type_traits>         // std::is_integral
#include "expr/expr_binaryop.h"
#include "expr/expr_cast.h"
#include "expr/expr_literal.h"
#include "utils/exceptions.h"
#include "utils/macros.h"
#include "column.h"
#include "column_impl.h"  // TODO: remove
#include "types.h"
namespace dt {
namespace expr {

static size_t id(Op opcode);
static size_t id(Op opcode, SType st1, SType st2);

static std::vector<std::string> binop_names;
static std::unordered_map<size_t, SType> binop_rules;

// Singleton instance
_binary_infos binary_infos;


enum OpMode {
  Error = 0,
  N_to_N = 1,
  N_to_One = 2,
  One_to_N = 3,
};

using mapperfn = void(*)(size_t row0, size_t row1, Column* cols);



//------------------------------------------------------------------------------
// Final mapper functions
//------------------------------------------------------------------------------

template<typename LT, typename RT, typename VT, VT (*OP)(LT, RT)>
static void map_n_to_n(size_t row0, size_t row1, Column* cols) {
  const LT* lhs_data = static_cast<const LT*>(cols[0]->data());
  const RT* rhs_data = static_cast<const RT*>(cols[1]->data());
  VT* res_data = static_cast<VT*>(cols[2]->data_w());
  for (size_t i = row0; i < row1; ++i) {
    res_data[i] = OP(lhs_data[i], rhs_data[i]);
  }
}

template<typename LT, typename RT, typename VT, VT (*OP)(LT, RT)>
static void map_n_to_1(size_t row0, size_t row1, Column* cols) {
  const LT* lhs_data = static_cast<const LT*>(cols[0]->data());
  RT rhs_value = static_cast<const RT*>(cols[1]->data())[0];
  VT* res_data = static_cast<VT*>(cols[2]->data_w());
  for (size_t i = row0; i < row1; ++i) {
    res_data[i] = OP(lhs_data[i], rhs_value);
  }
}

template<typename LT, typename RT, typename VT, VT (*OP)(LT, RT)>
static void map_1_to_n(size_t row0, size_t row1, Column* cols) {
  LT lhs_value = static_cast<const LT*>(cols[0]->data())[0];
  const RT* rhs_data = static_cast<const RT*>(cols[1]->data());
  VT* res_data = static_cast<VT*>(cols[2]->data_w());
  for (size_t i = row0; i < row1; ++i) {
    res_data[i] = OP(lhs_value, rhs_data[i]);
  }
}


template<typename TR, TR (*OP)(const CString&, bool, const CString&, bool)>
static void strmap_n_to_n(size_t row0, size_t row1, Column* cols) {
  const Column& col0 = cols[0];
  const Column& col1 = cols[1];
  auto res_data = static_cast<TR*>(cols[2]->data_w());
  CString val0, val1;
  bool isna0, isna1;
  for (size_t i = row0; i < row1; ++i) {
    isna0 = col0.get_element(i, &val0);
    isna1 = col1.get_element(i, &val1);
    res_data[i] = OP(val0, isna0, val1, isna1);
  }
}


template<typename TR, TR (*OP)(const CString&, bool, const CString&, bool)>
static void strmap_n_to_1(size_t row0, size_t row1, Column* cols) {
  const Column& col0 = cols[0];
  const Column& col1 = cols[1];
  auto res_data = static_cast<TR*>(cols[2]->data_w());
  CString val0, val1;
  bool isna0;
  bool isna1 = col1.get_element(0, &val1);
  for (size_t i = row0; i < row1; ++i) {
    isna0 = col0.get_element(i, &val0);
    res_data[i] = OP(val0, isna0, val1, isna1);
  }
}




//------------------------------------------------------------------------------
// Arithmetic operators
//------------------------------------------------------------------------------

template<typename LT, typename RT, typename VT>
inline static VT op_add(LT x, RT y) {
  return IsIntNA<LT>(x) || IsIntNA<RT>(y)? GETNA<VT>() : static_cast<VT>(x) + static_cast<VT>(y);
}

template<typename LT, typename RT, typename VT>
inline static VT op_sub(LT x, RT y) {
  return IsIntNA<LT>(x) || IsIntNA<RT>(y)? GETNA<VT>() : static_cast<VT>(x) - static_cast<VT>(y);
}

template<typename LT, typename RT, typename VT>
inline static VT op_mul(LT x, RT y) {
  return IsIntNA<LT>(x) || IsIntNA<RT>(y)? GETNA<VT>() : static_cast<VT>(x) * static_cast<VT>(y);
}

template<typename LT, typename RT, typename VT>
inline static VT op_div(LT x, RT y) {
  if (IsIntNA<LT>(x) || IsIntNA<RT>(y) || y == 0) return GETNA<VT>();
  VT vx = static_cast<VT>(x);
  VT vy = static_cast<VT>(y);
  VT res = vx / vy;
  if (std::is_integral<VT>::value && (vx < 0) != (vy < 0) && vx != res * vy) {
    --res;
  }
  return res;
}

template<typename LT, typename RT, typename VT>
struct Mod {
  inline static VT impl(LT x, RT y)  {
    if (IsIntNA<LT>(x) || IsIntNA<RT>(y) || y == 0) return GETNA<VT>();
    VT res = static_cast<VT>(x) % static_cast<VT>(y);
    if ((x < 0) != (y < 0) && res != 0) {
      res += static_cast<VT>(y);
    }
    return res;
  }
};

template<typename LT, typename RT>
struct Mod<LT, RT, float> {
  inline static float impl(LT x, RT y) {
    return y == 0? GETNA<float>() : std::fmod(static_cast<float>(x), static_cast<float>(y));
  }
};

template<typename LT, typename RT>
struct Mod<LT, RT, double> {
  inline static double impl(LT x, RT y) {
    return y == 0? GETNA<double>() : std::fmod(static_cast<double>(x), static_cast<double>(y));
  }
};


//------------------------------------------------------------------------------
// Relational operators
//------------------------------------------------------------------------------

template<typename LT, typename RT, typename VT>
inline static int8_t op_eq(LT x, RT y) {  // x == y
  bool x_isna = ISNA<LT>(x);
  bool y_isna = ISNA<RT>(y);
  return (!x_isna && !y_isna && static_cast<VT>(x) == static_cast<VT>(y)) ||
         (x_isna && y_isna);
}

template <typename T>
static int8_t op_eq1(T x, T y) {
  if (std::is_integral<T>::value) {
    return (x == y);
  } else {
    bool x_isna = ISNA<T>(x);
    bool y_isna = ISNA<T>(y);
    return (!x_isna && !y_isna && x == y) || (x_isna && y_isna);
  }
}

template<typename LT, typename RT, typename VT>
inline static int8_t op_ne(LT x, RT y) {  // x != y
  bool x_isna = ISNA<LT>(x);
  bool y_isna = ISNA<RT>(y);
  return (x_isna || y_isna || static_cast<VT>(x) != static_cast<VT>(y)) &&
         !(x_isna && y_isna);
}

template <typename T>
static int8_t op_ne1(T x, T y) {
  if (std::is_integral<T>::value) {
    return (x != y);
  } else {
    bool x_isna = ISNA<T>(x);
    bool y_isna = ISNA<T>(y);
    return (x_isna || y_isna || x != y) && !(x_isna && y_isna);
  }
}

template<typename LT, typename RT, typename VT>
inline static int8_t op_gt(LT x, RT y) {  // x > y
  bool x_isna = ISNA<LT>(x);
  bool y_isna = ISNA<RT>(y);
  return (!x_isna && !y_isna && static_cast<VT>(x) > static_cast<VT>(y));
}

template<typename LT, typename RT, typename VT>
inline static int8_t op_lt(LT x, RT y) {  // x < y
  bool x_isna = ISNA<LT>(x);
  bool y_isna = ISNA<RT>(y);
  return (!x_isna && !y_isna && static_cast<VT>(x) < static_cast<VT>(y));
}

template<typename LT, typename RT, typename VT>
inline static int8_t op_ge(LT x, RT y) {  // x >= y
  bool x_isna = ISNA<LT>(x);
  bool y_isna = ISNA<RT>(y);
  return (!x_isna && !y_isna && static_cast<VT>(x) >= static_cast<VT>(y)) ||
         (x_isna && y_isna);
}

template<typename LT, typename RT, typename VT>
inline static int8_t op_le(LT x, RT y) {  // x <= y
  bool x_isna = ISNA<LT>(x);
  bool y_isna = ISNA<RT>(y);
  return (!x_isna && !y_isna && static_cast<VT>(x) <= static_cast<VT>(y)) ||
         (x_isna && y_isna);
}

inline static int8_t strop_eq(const CString& a, bool a_isna,
                               const CString& b, bool b_isna) {
  return (a_isna || b_isna) ? (a_isna & b_isna)
                            : (a == b);
}

inline static int8_t strop_ne(const CString& a, bool a_isna,
                               const CString& b, bool b_isna) {
  return (a_isna || b_isna) ? !(a_isna & b_isna)
                            : !(a == b);
}



//------------------------------------------------------------------------------
// Logical operators
//------------------------------------------------------------------------------

inline static int8_t op_and(int8_t x, int8_t y) {
  bool x_isna = ISNA<int8_t>(x);
  bool y_isna = ISNA<int8_t>(y);
  return (x_isna || y_isna) ? GETNA<int8_t>() : (x && y);
}

inline static int8_t op_or(int8_t x, int8_t y) {
  bool x_isna = ISNA<int8_t>(x);
  bool y_isna = ISNA<int8_t>(y);
  return (x_isna || y_isna) ? GETNA<int8_t>() : (x || y);
}



//------------------------------------------------------------------------------
// Resolve the right mapping function
//------------------------------------------------------------------------------

template<typename LT, typename RT, typename VT, VT (*OP)(LT, RT)>
static mapperfn resolve2(OpMode mode) {
  switch (mode) {
    case N_to_N:   return map_n_to_n<LT, RT, VT, OP>;
    case N_to_One: return map_n_to_1<LT, RT, VT, OP>;
    case One_to_N: return map_1_to_n<LT, RT, VT, OP>;
    default:       return nullptr;
  }
}

template<typename TR, TR (*OP)(const CString&, bool, const CString&, bool)>
static mapperfn resolve2str(OpMode mode) {
  switch (mode) {
    case N_to_N:   return strmap_n_to_n<TR, OP>;
    case N_to_One: return strmap_n_to_1<TR, OP>;
    // case One_to_N: return map_1_to_n<TR, OP>;
    default:       return nullptr;
  }
}


template<typename LT, typename RT, typename VT>
static mapperfn resolve1(Op opcode, SType stype, Column* cols, size_t nrows, OpMode mode) {
  if (static_cast<size_t>(opcode) >= static_cast<size_t>(Op::EQ) &&
      static_cast<size_t>(opcode) <= static_cast<size_t>(Op::GE)) {
    // override stype for relational operators
    stype = SType::BOOL;
  } else if (opcode == Op::DIVIDE && std::is_integral<VT>::value) {
    stype = SType::FLOAT64;
  }
  cols[2] = Column::new_data_column(stype, nrows);
  switch (opcode) {
    case Op::PLUS:      return resolve2<LT, RT, VT, op_add<LT, RT, VT>>(mode);
    case Op::MINUS:     return resolve2<LT, RT, VT, op_sub<LT, RT, VT>>(mode);
    case Op::MULTIPLY:  return resolve2<LT, RT, VT, op_mul<LT, RT, VT>>(mode);
    case Op::INTDIV:    return resolve2<LT, RT, VT, op_div<LT, RT, VT>>(mode);
    case Op::MODULO:    return resolve2<LT, RT, VT, Mod<LT, RT, VT>::impl>(mode);
    case Op::DIVIDE:
      if (std::is_integral<VT>::value)
        return resolve2<LT, RT, double, op_div<LT, RT, double>>(mode);
      else
        return resolve2<LT, RT, VT, op_div<LT, RT, VT>>(mode);

    // Relational operators
    case Op::EQ: return resolve2<LT, RT, int8_t, op_eq<LT, RT, VT>>(mode);
    case Op::NE: return resolve2<LT, RT, int8_t, op_ne<LT, RT, VT>>(mode);
    case Op::GT: return resolve2<LT, RT, int8_t, op_gt<LT, RT, VT>>(mode);
    case Op::LT: return resolve2<LT, RT, int8_t, op_lt<LT, RT, VT>>(mode);
    case Op::GE: return resolve2<LT, RT, int8_t, op_ge<LT, RT, VT>>(mode);
    case Op::LE: return resolve2<LT, RT, int8_t, op_le<LT, RT, VT>>(mode);

    default: break;
  }
  return nullptr;
}


static mapperfn resolve1str(Op opcode, Column* cols, size_t nrows, OpMode mode) {
  if (mode == OpMode::One_to_N) {
    mode = OpMode::N_to_One;
    std::swap(cols[0], cols[1]);
  }
  cols[2] = Column::new_data_column(SType::BOOL, nrows);
  switch (opcode) {
    case Op::EQ: return resolve2str<int8_t, strop_eq>(mode);
    case Op::NE: return resolve2str<int8_t, strop_ne>(mode);
    default: break;
  }
  return nullptr;
}


static mapperfn resolve0(SType lhs_type, SType rhs_type, Op opcode, Column* cols, size_t nrows, OpMode mode) {
  if (mode == OpMode::Error) return nullptr;
  switch (lhs_type) {
    case SType::BOOL:
      if (rhs_type == SType::BOOL && (opcode == Op::AND || opcode == Op::OR)) {
        cols[2] = Column::new_data_column(SType::BOOL, nrows);
        if (opcode == Op::AND) return resolve2<int8_t, int8_t, int8_t, op_and>(mode);
        if (opcode == Op::OR)  return resolve2<int8_t, int8_t, int8_t, op_or>(mode);
      }
      FALLTHROUGH;

    case SType::INT8:
      switch (rhs_type) {
        case SType::BOOL:
        case SType::INT8:    return resolve1<int8_t, int8_t, int8_t>(opcode, SType::INT8, cols, nrows, mode);
        case SType::INT16:   return resolve1<int8_t, int16_t, int16_t>(opcode, SType::INT16, cols, nrows, mode);
        case SType::INT32:   return resolve1<int8_t, int32_t, int32_t>(opcode, SType::INT32, cols, nrows, mode);
        case SType::INT64:   return resolve1<int8_t, int64_t, int64_t>(opcode, SType::INT64, cols, nrows, mode);
        case SType::FLOAT32: return resolve1<int8_t, float, float>(opcode, SType::FLOAT32, cols, nrows, mode);
        case SType::FLOAT64: return resolve1<int8_t, double, double>(opcode, SType::FLOAT64, cols, nrows, mode);
        default: break;
      }
      break;

    case SType::INT16:
      switch (rhs_type) {
        case SType::BOOL:
        case SType::INT8:    return resolve1<int16_t, int8_t, int16_t>(opcode, SType::INT16, cols, nrows, mode);
        case SType::INT16:   return resolve1<int16_t, int16_t, int16_t>(opcode, SType::INT16, cols, nrows, mode);
        case SType::INT32:   return resolve1<int16_t, int32_t, int32_t>(opcode, SType::INT32, cols, nrows, mode);
        case SType::INT64:   return resolve1<int16_t, int64_t, int64_t>(opcode, SType::INT64, cols, nrows, mode);
        case SType::FLOAT32: return resolve1<int16_t, float, float>(opcode, SType::FLOAT32, cols, nrows, mode);
        case SType::FLOAT64: return resolve1<int16_t, double, double>(opcode, SType::FLOAT64, cols, nrows, mode);
        default: break;
      }
      break;

    case SType::INT32:
      switch (rhs_type) {
        case SType::BOOL:
        case SType::INT8:    return resolve1<int32_t, int8_t, int32_t>(opcode, SType::INT32, cols, nrows, mode);
        case SType::INT16:   return resolve1<int32_t, int16_t, int32_t>(opcode, SType::INT32, cols, nrows, mode);
        case SType::INT32:   return resolve1<int32_t, int32_t, int32_t>(opcode, SType::INT32, cols, nrows, mode);
        case SType::INT64:   return resolve1<int32_t, int64_t, int64_t>(opcode, SType::INT64, cols, nrows, mode);
        case SType::FLOAT32: return resolve1<int32_t, float, float>(opcode, SType::FLOAT32, cols, nrows, mode);
        case SType::FLOAT64: return resolve1<int32_t, double, double>(opcode, SType::FLOAT64, cols, nrows, mode);
        default: break;
      }
      break;

    case SType::INT64:
      switch (rhs_type) {
        case SType::BOOL:
        case SType::INT8:    return resolve1<int64_t, int8_t, int64_t>(opcode, SType::INT64, cols, nrows, mode);
        case SType::INT16:   return resolve1<int64_t, int16_t, int64_t>(opcode, SType::INT64, cols, nrows, mode);
        case SType::INT32:   return resolve1<int64_t, int32_t, int64_t>(opcode, SType::INT64, cols, nrows, mode);
        case SType::INT64:   return resolve1<int64_t, int64_t, int64_t>(opcode, SType::INT64, cols, nrows, mode);
        case SType::FLOAT32: return resolve1<int64_t, float, float>(opcode, SType::FLOAT32, cols, nrows, mode);
        case SType::FLOAT64: return resolve1<int64_t, double, double>(opcode, SType::FLOAT64, cols, nrows, mode);
        default: break;
      }
      break;

    case SType::FLOAT32:
      switch (rhs_type) {
        case SType::BOOL:
        case SType::INT8:    return resolve1<float, int8_t, float>(opcode, SType::FLOAT32, cols, nrows, mode);
        case SType::INT16:   return resolve1<float, int16_t, float>(opcode, SType::FLOAT32, cols, nrows, mode);
        case SType::INT32:   return resolve1<float, int32_t, float>(opcode, SType::FLOAT32, cols, nrows, mode);
        case SType::INT64:   return resolve1<float, int64_t, float>(opcode, SType::FLOAT32, cols, nrows, mode);
        case SType::FLOAT32: return resolve1<float, float, float>(opcode, SType::FLOAT32, cols, nrows, mode);
        case SType::FLOAT64: return resolve1<float, double, double>(opcode, SType::FLOAT64, cols, nrows, mode);
        default: break;
      }
      break;

    case SType::FLOAT64:
      switch (rhs_type) {
        case SType::BOOL:
        case SType::INT8:    return resolve1<double, int8_t, double>(opcode, SType::FLOAT64, cols, nrows, mode);
        case SType::INT16:   return resolve1<double, int16_t, double>(opcode, SType::FLOAT64, cols, nrows, mode);
        case SType::INT32:   return resolve1<double, int32_t, double>(opcode, SType::FLOAT64, cols, nrows, mode);
        case SType::INT64:   return resolve1<double, int64_t, double>(opcode, SType::FLOAT64, cols, nrows, mode);
        case SType::FLOAT32: return resolve1<double, float, double>(opcode, SType::FLOAT64, cols, nrows, mode);
        case SType::FLOAT64: return resolve1<double, double, double>(opcode, SType::FLOAT64, cols, nrows, mode);
        default: break;
      }
      break;

    case SType::STR32:
    case SType::STR64:
      switch (rhs_type) {
        case SType::STR32:
        case SType::STR64: return resolve1str(opcode, cols, nrows, mode);
        default: break;
      }
      break;

    default:
      break;
  }
  return nullptr;
}



//------------------------------------------------------------------------------
// binaryop
//------------------------------------------------------------------------------

static Column binaryop(Op opcode, Column& lhs, Column& rhs)
{
  // TODO: do not materialize, then `lhs` and `rhs` may be const
  lhs.materialize();
  rhs.materialize();
  size_t lhs_nrows = lhs.nrows();
  size_t rhs_nrows = rhs.nrows();
  if (lhs_nrows == 0 || rhs_nrows == 0) {
    lhs_nrows = rhs_nrows = 0;
  }
  size_t nrows = std::max(lhs_nrows, rhs_nrows);
  auto mode = (lhs_nrows == rhs_nrows)? OpMode::N_to_N :
              (rhs_nrows == 1)? OpMode::N_to_One :
              (lhs_nrows == 1)? OpMode::One_to_N : OpMode::Error;
  SType lhs_type = lhs.stype();
  SType rhs_type = rhs.stype();

  Column cols[3];
  cols[0] = lhs;  // shallow copy
  cols[1] = rhs;  // shallow copy

  mapperfn mapfn = nullptr;
  mapfn = resolve0(lhs_type, rhs_type, opcode, cols, nrows, mode);
  if (!mapfn) {
    throw RuntimeError()
      << "Unable to apply op " << static_cast<size_t>(opcode)
      << " to column1(stype=" << lhs_type
      << ", nrows=" << lhs.nrows() << ") and column2(stype=" << rhs_type
      << ", nrows=" << rhs.nrows() << ")";
  }
  (*mapfn)(0, nrows, cols);

  return std::move(cols[2]);
}




//------------------------------------------------------------------------------
// expr_binaryop
//------------------------------------------------------------------------------

expr_binaryop::expr_binaryop(pexpr&& l, pexpr&& r, Op op)
  : lhs(std::move(l)),
    rhs(std::move(r)),
    opcode(op) {}


SType expr_binaryop::resolve(const workframe& wf) {
  SType lhs_stype = lhs->resolve(wf);
  SType rhs_stype = rhs->resolve(wf);
  size_t triple = id(opcode, lhs_stype, rhs_stype);
  if (binop_rules.count(triple) == 0) {
    if (check_for_operation_with_literal_na(wf)) {
      return resolve(wf);  // Try resolving again, one of lhs/rhs has changed
    }
    throw TypeError() << "Binary operator `"
        << binop_names[id(opcode)]
        << "` cannot be applied to columns with stypes `" << lhs_stype
        << "` and `" << rhs_stype << "`";
  }
  return binop_rules.at(triple);
}


GroupbyMode expr_binaryop::get_groupby_mode(const workframe& wf) const {
  auto lmode = static_cast<uint8_t>(lhs->get_groupby_mode(wf));
  auto rmode = static_cast<uint8_t>(rhs->get_groupby_mode(wf));
  return static_cast<GroupbyMode>(std::max(lmode, rmode));
}


Column expr_binaryop::evaluate(workframe& wf) {
  auto lhs_res = lhs->evaluate(wf);
  auto rhs_res = rhs->evaluate(wf);
  return expr::binaryop(opcode, lhs_res, rhs_res);
}


// This function tries to solve the problem described in issue #1912: when
// a literal `None` is present in an expression, its type is ambiguous. We
// instantiate it into an stype BOOL, but that may not be correct: the
// expression may demand a different stype in that place.
//
// The "solution" created by this function is to detect the situations where
// one of the operands of a binary expression is literal `None`, and replace
// that expression with a cast into the stype of the other operand.
//
// A more robust approach would be to create a separate type (VOID) for a
// column created out of None values only, and have that type convert into
// any other stype as necessary.
//
bool expr_binaryop::check_for_operation_with_literal_na(const workframe& wf) {
  auto check_operand = [&](pexpr& arg) -> bool {
    auto pliteral = dynamic_cast<expr_literal*>(arg.get());
    if (!pliteral) return false;
    if (pliteral->resolve(wf) != SType::BOOL) return false;
    Column ocol = pliteral->evaluate(const_cast<workframe&>(wf));
    if (ocol.nrows() != 1) return false;
    return ISNA<int8_t>(reinterpret_cast<const int8_t*>(ocol->data())[0]);
  };
  if (check_operand(rhs)) {
    SType lhs_stype = lhs->resolve(wf);
    rhs = pexpr(new expr_cast(std::move(rhs), lhs_stype));
    return true;
  }
  if (check_operand(lhs)) {
    SType rhs_stype = rhs->resolve(wf);
    lhs = pexpr(new expr_cast(std::move(lhs), rhs_stype));
    return true;
  }
  return false;
}




//------------------------------------------------------------------------------
// one-time initialization
//------------------------------------------------------------------------------

static size_t id(Op opcode) {
  size_t res = static_cast<size_t>(opcode) - BINOP_FIRST;
  xassert(res < BINOP_COUNT);
  return res;
}

static size_t id(Op opcode, SType st1, SType st2) {
  return ((static_cast<size_t>(opcode) - BINOP_FIRST) << 16) +
         (static_cast<size_t>(st1) << 8) +
         (static_cast<size_t>(st2));
}

void init_binops() {
  constexpr SType bool8 = SType::BOOL;
  constexpr SType int8  = SType::INT8;
  constexpr SType int16 = SType::INT16;
  constexpr SType int32 = SType::INT32;
  constexpr SType int64 = SType::INT64;
  constexpr SType flt32 = SType::FLOAT32;
  constexpr SType flt64 = SType::FLOAT64;
  constexpr SType str32 = SType::STR32;
  constexpr SType str64 = SType::STR64;

  using styvec = std::vector<SType>;
  styvec integer_stypes = {int8, int16, int32, int64};
  styvec numeric_stypes = {bool8, int8, int16, int32, int64, flt32, flt64};
  styvec string_types = {str32, str64};

  for (SType st1 : numeric_stypes) {
    for (SType st2 : numeric_stypes) {
      SType stm = std::max(st1, st2);
      binop_rules[id(Op::PLUS, st1, st2)] = stm;
      binop_rules[id(Op::MINUS, st1, st2)] = stm;
      binop_rules[id(Op::MULTIPLY, st1, st2)] = stm;
      binop_rules[id(Op::POWER, st1, st2)] = stm;
      binop_rules[id(Op::DIVIDE, st1, st2)] = flt64;
      binop_rules[id(Op::EQ, st1, st2)] = bool8;
      binop_rules[id(Op::NE, st1, st2)] = bool8;
      binop_rules[id(Op::LT, st1, st2)] = bool8;
      binop_rules[id(Op::GT, st1, st2)] = bool8;
      binop_rules[id(Op::LE, st1, st2)] = bool8;
      binop_rules[id(Op::GE, st1, st2)] = bool8;
    }
  }
  for (SType st1 : integer_stypes) {
    for (SType st2 : integer_stypes) {
      SType stm = std::max(st1, st2);
      binop_rules[id(Op::INTDIV, st1, st2)] = stm;
      binop_rules[id(Op::MODULO, st1, st2)] = stm;
      binop_rules[id(Op::LSHIFT, st1, st2)] = stm;
      binop_rules[id(Op::RSHIFT, st1, st2)] = stm;
    }
  }
  for (SType st1 : string_types) {
    for (SType st2 : string_types) {
      binop_rules[id(Op::EQ, st1, st2)] = bool8;
      binop_rules[id(Op::NE, st1, st2)] = bool8;
    }
  }
  binop_rules[id(Op::AND, bool8, bool8)] = bool8;
  binop_rules[id(Op::OR, bool8, bool8)] = bool8;

  binop_names.resize(BINOP_COUNT);
  binop_names[id(Op::PLUS)] = "+";
  binop_names[id(Op::MINUS)] = "-";
  binop_names[id(Op::MULTIPLY)] = "*";
  binop_names[id(Op::DIVIDE)] = "/";
  binop_names[id(Op::INTDIV)] = "//";
  binop_names[id(Op::POWER)] = "**";
  binop_names[id(Op::MODULO)] = "%";
  binop_names[id(Op::AND)] = "&";
  binop_names[id(Op::OR)] = "|";
  binop_names[id(Op::LSHIFT)] = "<<";
  binop_names[id(Op::RSHIFT)] = ">>";
  binop_names[id(Op::EQ)] = "==";
  binop_names[id(Op::NE)] = "!=";
  binop_names[id(Op::GT)] = ">";
  binop_names[id(Op::LT)] = "<";
  binop_names[id(Op::GE)] = ">=";
  binop_names[id(Op::LE)] = "<=";
}




//------------------------------------------------------------------------------
// binary_infos
//------------------------------------------------------------------------------
using erased_func_t = _binary_infos::erased_func_t;

constexpr size_t _binary_infos::id(Op opcode) noexcept {
  return static_cast<size_t>(opcode) - BINOP_FIRST;
}

constexpr size_t _binary_infos::id(Op op, SType stype1, SType stype2) noexcept {
  return ((static_cast<size_t>(op) - BINOP_FIRST) << 16) +
         (static_cast<size_t>(stype1) << 8) +
         static_cast<size_t>(stype2);
}


template <typename T>
static erased_func_t resolve_op2(Op op) {
  switch (op) {
    case Op::EQ: return reinterpret_cast<erased_func_t>(op_eq1<T>);
    case Op::NE: return reinterpret_cast<erased_func_t>(op_ne1<T>);
    default: return nullptr;
  }
}

static erased_func_t resolve_op(Op op, SType stype) {
  switch (stype) {
    case SType::INT8:    return resolve_op2<int8_t>(op);
    case SType::INT16:   return resolve_op2<int16_t>(op);
    case SType::INT32:   return resolve_op2<int32_t>(op);
    case SType::INT64:   return resolve_op2<int64_t>(op);
    case SType::FLOAT32: return resolve_op2<float>(op);
    case SType::FLOAT64: return resolve_op2<double>(op);
    case SType::STR32:
    case SType::STR64:   return resolve_op2<CString>(op);
    default: return nullptr;  // LCOV_EXCL_LINE
  }
}

void _binary_infos::add_relop(Op op, const char* name) {
  static SType stypes[] = {
    SType::BOOL, SType::INT8, SType::INT16, SType::INT32, SType::INT64,
    SType::FLOAT32, SType::FLOAT64, SType::STR32, SType::STR64
  };
  for (int i = 0; i < 9; ++i) {
    for (int j = 0; j < 9; ++ j) {
      if ((i < 7) != (j < 7)) continue;  // strings do not mix with numbers
      int m = std::max(1, std::max(i, j));
      size_t entry_id = id(op, stypes[i], stypes[j]);
      infos[entry_id] = {
        resolve_op(op, stypes[m]),
        SType::BOOL,  // output_stype
        (i == m || i >= 7)? SType::VOID : stypes[m],
        (j == m || j >= 7)? SType::VOID : stypes[m]
      };
    }
  }
  names[id(op)] = name;
}


_binary_infos::_binary_infos() {
  add_relop(Op::EQ, "==");
  add_relop(Op::NE, "!=");
  add_relop(Op::GT, ">");
  add_relop(Op::LT, "<");
  add_relop(Op::GE, ">=");
  add_relop(Op::LE, "<=");
}




}}  // namespace dt::expr
