//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
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
#include "expr/py_expr.h"
#include <cmath>               // std::fmod
#include <type_traits>         // std::is_integral
#include "types.h"
#include "utils/exceptions.h"


// Should be in sync with a map in binary_expr.py
enum OpCode {
  Plus           = 1,
  Minus          = 2,
  Multiply       = 3,
  Divide         = 4,
  IntDivide      = 5,
  Power          = 6,
  Modulo         = 7,
  LogicalAnd     = 8,
  LogicalOr      = 9,
  LeftShift      = 10,
  RightShift     = 11,
  Equal          = 12,  // ==
  NotEqual       = 13,  // !=
  Greater        = 14,  // >
  Less           = 15,  // <
  GreaterOrEqual = 16,  // >=
  LessOrEqual    = 17,  // <=
};

typedef void (*mapperfn)(int64_t row0, int64_t row1, void** params);




template<typename LT, typename RT, typename VT, VT (*OP)(LT, RT)>
static void resolve2(int64_t row0, int64_t row1, void** params) {
  LT* lhs_data = static_cast<LT*>(static_cast<Column*>(params[0])->data());
  RT* rhs_data = static_cast<RT*>(static_cast<Column*>(params[1])->data());
  VT* res_data = static_cast<VT*>(static_cast<Column*>(params[2])->data());
  for (int64_t i = row0; i < row1; ++i) {
    res_data[i] = OP(lhs_data[i], rhs_data[i]);
  }
}


template<typename LT, typename RT, typename VT> inline static VT op_add(LT x, RT y) { return IsIntNA<LT>(x) || IsIntNA<RT>(y)? GETNA<VT>() : static_cast<VT>(x) + static_cast<VT>(y); }
template<typename LT, typename RT, typename VT> inline static VT op_sub(LT x, RT y) { return IsIntNA<LT>(x) || IsIntNA<RT>(y)? GETNA<VT>() : static_cast<VT>(x) - static_cast<VT>(y); }
template<typename LT, typename RT, typename VT> inline static VT op_mul(LT x, RT y) { return IsIntNA<LT>(x) || IsIntNA<RT>(y)? GETNA<VT>() : static_cast<VT>(x) * static_cast<VT>(y); }
template<typename LT, typename RT, typename VT> inline static VT op_div(LT x, RT y) { return IsIntNA<LT>(x) || IsIntNA<RT>(y) || y == 0? GETNA<VT>() : static_cast<VT>(x) / static_cast<VT>(y); }

template<typename LT, typename RT, typename VT>
struct Mod {
  inline static VT impl(LT x, RT y)  {
    return IsIntNA<LT>(x) || IsIntNA<RT>(y) || y == 0? GETNA<VT>() : static_cast<VT>(x) % static_cast<VT>(y);
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

template<typename LT, typename RT>
inline static int8_t op_eq(LT x, RT y) {  // x == y
  bool x_isna = ISNA<LT>(x);
  bool y_isna = ISNA<RT>(y);
  return (!x_isna && !y_isna && x == y) || (x_isna && y_isna);
}

template<typename LT, typename RT>
inline static int8_t op_ne(LT x, RT y) {  // x != y
  bool x_isna = ISNA<LT>(x);
  bool y_isna = ISNA<RT>(y);
  return (x_isna || y_isna || x != y) && !(x_isna && y_isna);
}

template<typename LT, typename RT>
inline static int8_t op_gt(LT x, RT y) {  // x > y
  bool x_isna = ISNA<LT>(x);
  bool y_isna = ISNA<RT>(y);
  return (!x_isna && !y_isna && x > y);
}

template<typename LT, typename RT>
inline static int8_t op_lt(LT x, RT y) {  // x < y
  bool x_isna = ISNA<LT>(x);
  bool y_isna = ISNA<RT>(y);
  return (!x_isna && !y_isna && x < y);
}

template<typename LT, typename RT>
inline static int8_t op_ge(LT x, RT y) {  // x >= y
  bool x_isna = ISNA<LT>(x);
  bool y_isna = ISNA<RT>(y);
  return (!x_isna && !y_isna && x >= y) || (x_isna && y_isna);
}

template<typename LT, typename RT>
inline static int8_t op_le(LT x, RT y) {  // x <= y
  bool x_isna = ISNA<LT>(x);
  bool y_isna = ISNA<RT>(y);
  return (!x_isna && !y_isna && x <= y) || (x_isna && y_isna);
}




template<typename LT, typename RT, typename VT>
inline static mapperfn resolve0(int opcode, SType stype, void** params) {
  if (opcode >= OpCode::Equal) {
    // override stype for relational operators
    stype = ST_BOOLEAN_I1;
  }
  int64_t nrows = static_cast<Column*>(params[0])->nrows;
  params[2] = Column::new_data_column(stype, nrows);
  switch (opcode) {
    case OpCode::Plus:      return resolve2<LT, RT, VT, op_add<LT, RT, VT>>;
    case OpCode::Minus:     return resolve2<LT, RT, VT, op_sub<LT, RT, VT>>;
    case OpCode::Multiply:  return resolve2<LT, RT, VT, op_mul<LT, RT, VT>>;
    case OpCode::IntDivide: return resolve2<LT, RT, VT, op_div<LT, RT, VT>>;
    case OpCode::Modulo:    return resolve2<LT, RT, VT, Mod<LT, RT, VT>::impl>;
    case OpCode::Divide:
      if (std::is_integral<VT>::value)
        return resolve2<LT, RT, double, op_div<LT, RT, double>>;
      else
        return resolve2<LT, RT, VT, op_div<LT, RT, VT>>;

    // Relational operators
    case OpCode::Equal:          return resolve2<LT, RT, int8_t, op_eq<LT, RT>>;
    case OpCode::NotEqual:       return resolve2<LT, RT, int8_t, op_ne<LT, RT>>;
    case OpCode::Greater:        return resolve2<LT, RT, int8_t, op_gt<LT, RT>>;
    case OpCode::Less:           return resolve2<LT, RT, int8_t, op_lt<LT, RT>>;
    case OpCode::GreaterOrEqual: return resolve2<LT, RT, int8_t, op_ge<LT, RT>>;
    case OpCode::LessOrEqual:    return resolve2<LT, RT, int8_t, op_le<LT, RT>>;
  }
  return nullptr;
}


static mapperfn resolve_n_to_n(SType lhs_type, SType rhs_type, int opcode, void** params) {
  switch (lhs_type) {
    case ST_BOOLEAN_I1:
    case ST_INTEGER_I1:
      switch (rhs_type) {
        case ST_BOOLEAN_I1:
        case ST_INTEGER_I1: return resolve0<int8_t, int8_t, int8_t>(opcode, ST_INTEGER_I1, params);
        case ST_INTEGER_I2: return resolve0<int8_t, int16_t, int16_t>(opcode, ST_INTEGER_I2, params);
        case ST_INTEGER_I4: return resolve0<int8_t, int32_t, int32_t>(opcode, ST_INTEGER_I4, params);
        case ST_INTEGER_I8: return resolve0<int8_t, int64_t, int64_t>(opcode, ST_INTEGER_I8, params);
        case ST_REAL_F4:    return resolve0<int8_t, float, float>(opcode, ST_REAL_F4, params);
        case ST_REAL_F8:    return resolve0<int8_t, double, double>(opcode, ST_REAL_F8, params);
        default: break;
      }
      break;

    case ST_INTEGER_I2:
      switch (rhs_type) {
        case ST_BOOLEAN_I1:
        case ST_INTEGER_I1: return resolve0<int16_t, int8_t, int16_t>(opcode, ST_INTEGER_I2, params);
        case ST_INTEGER_I2: return resolve0<int16_t, int16_t, int16_t>(opcode, ST_INTEGER_I2, params);
        case ST_INTEGER_I4: return resolve0<int16_t, int32_t, int32_t>(opcode, ST_INTEGER_I4, params);
        case ST_INTEGER_I8: return resolve0<int16_t, int64_t, int64_t>(opcode, ST_INTEGER_I8, params);
        case ST_REAL_F4:    return resolve0<int16_t, float, float>(opcode, ST_REAL_F4, params);
        case ST_REAL_F8:    return resolve0<int16_t, double, double>(opcode, ST_REAL_F8, params);
        default: break;
      }
      break;

    case ST_INTEGER_I4:
      switch (rhs_type) {
        case ST_BOOLEAN_I1:
        case ST_INTEGER_I1: return resolve0<int32_t, int8_t, int32_t>(opcode, ST_INTEGER_I4, params);
        case ST_INTEGER_I2: return resolve0<int32_t, int16_t, int32_t>(opcode, ST_INTEGER_I4, params);
        case ST_INTEGER_I4: return resolve0<int32_t, int32_t, int32_t>(opcode, ST_INTEGER_I4, params);
        case ST_INTEGER_I8: return resolve0<int32_t, int64_t, int64_t>(opcode, ST_INTEGER_I8, params);
        case ST_REAL_F4:    return resolve0<int32_t, float, float>(opcode, ST_REAL_F4, params);
        case ST_REAL_F8:    return resolve0<int32_t, double, double>(opcode, ST_REAL_F8, params);
        default: break;
      }
      break;

    case ST_INTEGER_I8:
      switch (rhs_type) {
        case ST_BOOLEAN_I1:
        case ST_INTEGER_I1: return resolve0<int64_t, int8_t, int64_t>(opcode, ST_INTEGER_I8, params);
        case ST_INTEGER_I2: return resolve0<int64_t, int16_t, int64_t>(opcode, ST_INTEGER_I8, params);
        case ST_INTEGER_I4: return resolve0<int64_t, int32_t, int64_t>(opcode, ST_INTEGER_I8, params);
        case ST_INTEGER_I8: return resolve0<int64_t, int64_t, int64_t>(opcode, ST_INTEGER_I8, params);
        case ST_REAL_F4:    return resolve0<int64_t, float, float>(opcode, ST_REAL_F4, params);
        case ST_REAL_F8:    return resolve0<int64_t, double, double>(opcode, ST_REAL_F8, params);
        default: break;
      }
      break;

    case ST_REAL_F4:
      switch (rhs_type) {
        case ST_BOOLEAN_I1:
        case ST_INTEGER_I1: return resolve0<float, int8_t, float>(opcode, ST_REAL_F4, params);
        case ST_INTEGER_I2: return resolve0<float, int16_t, float>(opcode, ST_REAL_F4, params);
        case ST_INTEGER_I4: return resolve0<float, int32_t, float>(opcode, ST_REAL_F4, params);
        case ST_INTEGER_I8: return resolve0<float, int64_t, float>(opcode, ST_REAL_F4, params);
        case ST_REAL_F4:    return resolve0<float, float, float>(opcode, ST_REAL_F4, params);
        case ST_REAL_F8:    return resolve0<float, double, double>(opcode, ST_REAL_F8, params);
        default: break;
      }
      break;

    case ST_REAL_F8:
      switch (rhs_type) {
        case ST_BOOLEAN_I1:
        case ST_INTEGER_I1: return resolve0<double, int8_t, double>(opcode, ST_REAL_F8, params);
        case ST_INTEGER_I2: return resolve0<double, int16_t, double>(opcode, ST_REAL_F8, params);
        case ST_INTEGER_I4: return resolve0<double, int32_t, double>(opcode, ST_REAL_F8, params);
        case ST_INTEGER_I8: return resolve0<double, int64_t, double>(opcode, ST_REAL_F8, params);
        case ST_REAL_F4:    return resolve0<double, float, double>(opcode, ST_REAL_F8, params);
        case ST_REAL_F8:    return resolve0<double, double, double>(opcode, ST_REAL_F8, params);
        default: break;
      }
      break;

    default:
      break;
  }
  return nullptr;
}


//------------------------------------------------------------------------------
// Exported binaryop function
//------------------------------------------------------------------------------

Column* expr::binaryop(int opcode, Column* lhs, Column* rhs)
{
  int64_t lhs_nrows = lhs->nrows;
  int64_t rhs_nrows = rhs->nrows;
  SType lhs_type = lhs->stype();
  SType rhs_type = rhs->stype();
  void* params[3];
  params[0] = lhs;
  params[1] = rhs;
  params[2] = nullptr;

  mapperfn mapfn = nullptr;
  if (lhs_nrows == rhs_nrows) {
    mapfn = resolve_n_to_n(lhs_type, rhs_type, opcode, params);
  }
  if (!mapfn) {
    throw RuntimeError()
      << "Unable to apply op " << opcode << " to column1(stype=" << lhs_type
      << ", nrows=" << lhs_nrows << ") and column2(stype=" << rhs_type
      << ", nrows=" << rhs_nrows << ")";
  }

  int64_t nrows = std::max(lhs_nrows, rhs_nrows);
  (*mapfn)(0, nrows, params);

  return static_cast<Column*>(params[2]);
}
