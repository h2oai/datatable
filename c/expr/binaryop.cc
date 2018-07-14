//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
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


namespace expr
{

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

enum OpMode {
  Error = 0,
  N_to_N = 1,
  N_to_One = 2,
  One_to_N = 3,
};



//------------------------------------------------------------------------------
// Final mapper functions
//------------------------------------------------------------------------------

template<typename LT, typename RT, typename VT, VT (*OP)(LT, RT)>
static void map_n_to_n(int64_t row0, int64_t row1, void** params) {
  Column* col0 = static_cast<Column*>(params[0]);
  Column* col1 = static_cast<Column*>(params[1]);
  Column* col2 = static_cast<Column*>(params[2]);
  const LT* lhs_data = static_cast<const LT*>(col0->data());
  const RT* rhs_data = static_cast<const RT*>(col1->data());
  VT* res_data = static_cast<VT*>(col2->data_w());
  for (int64_t i = row0; i < row1; ++i) {
    res_data[i] = OP(lhs_data[i], rhs_data[i]);
  }
}

template<typename LT, typename RT, typename VT, VT (*OP)(LT, RT)>
static void map_n_to_1(int64_t row0, int64_t row1, void** params) {
  Column* col0 = static_cast<Column*>(params[0]);
  Column* col1 = static_cast<Column*>(params[1]);
  Column* col2 = static_cast<Column*>(params[2]);
  const LT* lhs_data = static_cast<const LT*>(col0->data());
  RT rhs_value = static_cast<const RT*>(col1->data())[0];
  VT* res_data = static_cast<VT*>(col2->data_w());
  for (int64_t i = row0; i < row1; ++i) {
    res_data[i] = OP(lhs_data[i], rhs_value);
  }
}

template<typename LT, typename RT, typename VT, VT (*OP)(LT, RT)>
static void map_1_to_n(int64_t row0, int64_t row1, void** params) {
  Column* col0 = static_cast<Column*>(params[0]);
  Column* col1 = static_cast<Column*>(params[1]);
  Column* col2 = static_cast<Column*>(params[2]);
  LT lhs_value = static_cast<const LT*>(col0->data())[0];
  const RT* rhs_data = static_cast<const RT*>(col1->data());
  VT* res_data = static_cast<VT*>(col2->data_w());
  for (int64_t i = row0; i < row1; ++i) {
    res_data[i] = OP(lhs_value, rhs_data[i]);
  }
}


template<typename T0, typename T1, typename T2,
         T2 (*OP)(T0, T0, const char*, T1, T1, const char*)>
static void strmap_n_to_n(int64_t row0, int64_t row1, void** params) {
  auto col0 = static_cast<StringColumn<T0>*>(params[0]);
  auto col1 = static_cast<StringColumn<T1>*>(params[1]);
  auto col2 = static_cast<Column*>(params[2]);
  const T0* offsets0 = col0->offsets();
  const T1* offsets1 = col1->offsets();
  const char* strdata0 = col0->strdata();
  const char* strdata1 = col1->strdata();
  T2* res_data = static_cast<T2*>(col2->data_w());
  T0 str0start = offsets0[row0 - 1] & ~GETNA<T0>();
  T1 str1start = offsets1[row0 - 1] & ~GETNA<T1>();
  for (int64_t i = row0; i < row1; ++i) {
    T0 str0end = offsets0[i];
    T1 str1end = offsets1[i];
    res_data[i] = OP(str0start, str0end, strdata0,
                     str1start, str1end, strdata1);
    str0start = str0end & ~GETNA<T0>();
    str1start = str1end & ~GETNA<T1>();
  }
}


template<typename T0, typename T1, typename T2,
         T2 (*OP)(T0, T0, const char*, T1, T1, const char*)>
static void strmap_n_to_1(int64_t row0, int64_t row1, void** params) {
  auto col0 = static_cast<StringColumn<T0>*>(params[0]);
  auto col1 = static_cast<StringColumn<T1>*>(params[1]);
  auto col2 = static_cast<Column*>(params[2]);
  const T0* offsets0 = col0->offsets();
  const T1* offsets1 = col1->offsets();
  const char* strdata0 = col0->strdata();
  const char* strdata1 = col1->strdata();
  T0 str0start = offsets0[row0 - 1] & ~GETNA<T0>();
  T1 str1start = 0;
  T1 str1end = offsets1[0];
  T2* res_data = static_cast<T2*>(col2->data_w());
  for (int64_t i = row0; i < row1; ++i) {
    T0 str0end = offsets0[i];
    res_data[i] = OP(str0start, str0end, strdata0,
                     str1start, str1end, strdata1);
    str0start = str0end & ~GETNA<T0>();
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
  if (vx < 0 != vy < 0 && vx != res * vy) {
    --res;
  }
  return res;
}

template<typename LT, typename RT, typename VT>
struct Mod {
  inline static VT impl(LT x, RT y)  {
    if (IsIntNA<LT>(x) || IsIntNA<RT>(y) || y == 0) return GETNA<VT>();
    VT res = static_cast<VT>(x) % static_cast<VT>(y);
    if (x < 0 != y < 0 && res != 0) {
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

template<typename LT, typename RT, typename VT>
inline static int8_t op_ne(LT x, RT y) {  // x != y
  bool x_isna = ISNA<LT>(x);
  bool y_isna = ISNA<RT>(y);
  return (x_isna || y_isna || static_cast<VT>(x) != static_cast<VT>(y)) &&
         !(x_isna && y_isna);
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

template<typename T1, typename T2>
inline static int8_t strop_eq(T1 start1, T1 end1, const char* strdata1,
                              T2 start2, T2 end2, const char* strdata2) {
  if (!ISNA<T1>(end1) && !ISNA<T2>(end2)) {
    if (end1 - start1 == end2 - start2) {
      const char* ch1 = strdata1 + start1;
      const char* ch2 = strdata2 + start2;
      const char* end = strdata1 + end1;
      while (ch1 < end) {
        if (*ch1++ != *ch2++) return 0;
      }
      return 1;
    } else {
      return 0;
    }
  } else {
    return ISNA<T1>(end1) && ISNA<T2>(end2);
  }
}

template<typename T1, typename T2>
inline static int8_t strop_ne(T1 start1, T1 end1, const char* strdata1,
                              T2 start2, T2 end2, const char* strdata2) {
  if (!ISNA<T1>(end1) && !ISNA<T2>(end2)) {
    if (end1 - start1 == end2 - start2) {
      const char* ch1 = strdata1 + start1;
      const char* ch2 = strdata2 + start2;
      const char* end = strdata1 + end1;
      while (ch1 < end) {
        if (*ch1++ != *ch2++) return 1;
      }
      return 0;
    } else {
      return 1;
    }
  } else {
    return !(ISNA<T1>(end1) && ISNA<T2>(end2));
  }
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
    case Error:    return nullptr;

  }
}

template<typename T0, typename T1, typename T2,
         T2 (*OP)(T0, T0, const char*, T1, T1, const char*)>
static mapperfn resolve2str(OpMode mode) {
  switch (mode) {
    case N_to_N:   return strmap_n_to_n<T0, T1, T2, OP>;
    case N_to_One: return strmap_n_to_1<T0, T1, T2, OP>;
    // case One_to_N: return map_1_to_n<T0, T1, T2, OP>;
    default:       return nullptr;
  }
}


template<typename LT, typename RT, typename VT>
static mapperfn resolve1(int opcode, SType stype, void** params, int64_t nrows, OpMode mode) {
  if (opcode >= OpCode::Equal) {
    // override stype for relational operators
    stype = SType::BOOL;
  } else if (opcode == OpCode::Divide && std::is_integral<VT>::value) {
    stype = SType::FLOAT64;
  }
  params[2] = Column::new_data_column(stype, nrows);
  switch (opcode) {
    case OpCode::Plus:      return resolve2<LT, RT, VT, op_add<LT, RT, VT>>(mode);
    case OpCode::Minus:     return resolve2<LT, RT, VT, op_sub<LT, RT, VT>>(mode);
    case OpCode::Multiply:  return resolve2<LT, RT, VT, op_mul<LT, RT, VT>>(mode);
    case OpCode::IntDivide: return resolve2<LT, RT, VT, op_div<LT, RT, VT>>(mode);
    case OpCode::Modulo:    return resolve2<LT, RT, VT, Mod<LT, RT, VT>::impl>(mode);
    case OpCode::Divide:
      if (std::is_integral<VT>::value)
        return resolve2<LT, RT, double, op_div<LT, RT, double>>(mode);
      else
        return resolve2<LT, RT, VT, op_div<LT, RT, VT>>(mode);

    // Relational operators
    case OpCode::Equal:          return resolve2<LT, RT, int8_t, op_eq<LT, RT, VT>>(mode);
    case OpCode::NotEqual:       return resolve2<LT, RT, int8_t, op_ne<LT, RT, VT>>(mode);
    case OpCode::Greater:        return resolve2<LT, RT, int8_t, op_gt<LT, RT, VT>>(mode);
    case OpCode::Less:           return resolve2<LT, RT, int8_t, op_lt<LT, RT, VT>>(mode);
    case OpCode::GreaterOrEqual: return resolve2<LT, RT, int8_t, op_ge<LT, RT, VT>>(mode);
    case OpCode::LessOrEqual:    return resolve2<LT, RT, int8_t, op_le<LT, RT, VT>>(mode);
  }
  delete static_cast<Column*>(params[2]);
  return nullptr;
}


template<typename T0, typename T1>
static mapperfn resolve1str(int opcode, void** params, int64_t nrows, OpMode mode) {
  if (mode == OpMode::One_to_N) {
    mode = OpMode::N_to_One;
    std::swap(params[0], params[1]);
  }
  params[2] = Column::new_data_column(SType::BOOL, nrows);
  switch (opcode) {
    case OpCode::Equal:    return resolve2str<T0, T1, int8_t, strop_eq<T0, T1>>(mode);
    case OpCode::NotEqual: return resolve2str<T0, T1, int8_t, strop_ne<T0, T1>>(mode);
  }
  delete static_cast<Column*>(params[2]);
  return nullptr;
}


static mapperfn resolve0(SType lhs_type, SType rhs_type, int opcode, void** params, int64_t nrows, OpMode mode) {
  if (mode == OpMode::Error) return nullptr;
  switch (lhs_type) {
    case SType::BOOL:
      if (rhs_type == SType::BOOL && (opcode == OpCode::LogicalAnd ||
                                      opcode == OpCode::LogicalOr)) {
        params[2] = Column::new_data_column(SType::BOOL, nrows);
        if (opcode == OpCode::LogicalAnd) return resolve2<int8_t, int8_t, int8_t, op_and>(mode);
        if (opcode == OpCode::LogicalOr)  return resolve2<int8_t, int8_t, int8_t, op_or>(mode);
      }
      [[clang::fallthrough]];

    case SType::INT8:
      switch (rhs_type) {
        case SType::BOOL:
        case SType::INT8:    return resolve1<int8_t, int8_t, int8_t>(opcode, SType::INT8, params, nrows, mode);
        case SType::INT16:   return resolve1<int8_t, int16_t, int16_t>(opcode, SType::INT16, params, nrows, mode);
        case SType::INT32:   return resolve1<int8_t, int32_t, int32_t>(opcode, SType::INT32, params, nrows, mode);
        case SType::INT64:   return resolve1<int8_t, int64_t, int64_t>(opcode, SType::INT64, params, nrows, mode);
        case SType::FLOAT32: return resolve1<int8_t, float, float>(opcode, SType::FLOAT32, params, nrows, mode);
        case SType::FLOAT64: return resolve1<int8_t, double, double>(opcode, SType::FLOAT64, params, nrows, mode);
        default: break;
      }
      break;

    case SType::INT16:
      switch (rhs_type) {
        case SType::BOOL:
        case SType::INT8:    return resolve1<int16_t, int8_t, int16_t>(opcode, SType::INT16, params, nrows, mode);
        case SType::INT16:   return resolve1<int16_t, int16_t, int16_t>(opcode, SType::INT16, params, nrows, mode);
        case SType::INT32:   return resolve1<int16_t, int32_t, int32_t>(opcode, SType::INT32, params, nrows, mode);
        case SType::INT64:   return resolve1<int16_t, int64_t, int64_t>(opcode, SType::INT64, params, nrows, mode);
        case SType::FLOAT32: return resolve1<int16_t, float, float>(opcode, SType::FLOAT32, params, nrows, mode);
        case SType::FLOAT64: return resolve1<int16_t, double, double>(opcode, SType::FLOAT64, params, nrows, mode);
        default: break;
      }
      break;

    case SType::INT32:
      switch (rhs_type) {
        case SType::BOOL:
        case SType::INT8:    return resolve1<int32_t, int8_t, int32_t>(opcode, SType::INT32, params, nrows, mode);
        case SType::INT16:   return resolve1<int32_t, int16_t, int32_t>(opcode, SType::INT32, params, nrows, mode);
        case SType::INT32:   return resolve1<int32_t, int32_t, int32_t>(opcode, SType::INT32, params, nrows, mode);
        case SType::INT64:   return resolve1<int32_t, int64_t, int64_t>(opcode, SType::INT64, params, nrows, mode);
        case SType::FLOAT32: return resolve1<int32_t, float, float>(opcode, SType::FLOAT32, params, nrows, mode);
        case SType::FLOAT64: return resolve1<int32_t, double, double>(opcode, SType::FLOAT64, params, nrows, mode);
        default: break;
      }
      break;

    case SType::INT64:
      switch (rhs_type) {
        case SType::BOOL:
        case SType::INT8:    return resolve1<int64_t, int8_t, int64_t>(opcode, SType::INT64, params, nrows, mode);
        case SType::INT16:   return resolve1<int64_t, int16_t, int64_t>(opcode, SType::INT64, params, nrows, mode);
        case SType::INT32:   return resolve1<int64_t, int32_t, int64_t>(opcode, SType::INT64, params, nrows, mode);
        case SType::INT64:   return resolve1<int64_t, int64_t, int64_t>(opcode, SType::INT64, params, nrows, mode);
        case SType::FLOAT32: return resolve1<int64_t, float, float>(opcode, SType::FLOAT32, params, nrows, mode);
        case SType::FLOAT64: return resolve1<int64_t, double, double>(opcode, SType::FLOAT64, params, nrows, mode);
        default: break;
      }
      break;

    case SType::FLOAT32:
      switch (rhs_type) {
        case SType::BOOL:
        case SType::INT8:    return resolve1<float, int8_t, float>(opcode, SType::FLOAT32, params, nrows, mode);
        case SType::INT16:   return resolve1<float, int16_t, float>(opcode, SType::FLOAT32, params, nrows, mode);
        case SType::INT32:   return resolve1<float, int32_t, float>(opcode, SType::FLOAT32, params, nrows, mode);
        case SType::INT64:   return resolve1<float, int64_t, float>(opcode, SType::FLOAT32, params, nrows, mode);
        case SType::FLOAT32: return resolve1<float, float, float>(opcode, SType::FLOAT32, params, nrows, mode);
        case SType::FLOAT64: return resolve1<float, double, double>(opcode, SType::FLOAT64, params, nrows, mode);
        default: break;
      }
      break;

    case SType::FLOAT64:
      switch (rhs_type) {
        case SType::BOOL:
        case SType::INT8:    return resolve1<double, int8_t, double>(opcode, SType::FLOAT64, params, nrows, mode);
        case SType::INT16:   return resolve1<double, int16_t, double>(opcode, SType::FLOAT64, params, nrows, mode);
        case SType::INT32:   return resolve1<double, int32_t, double>(opcode, SType::FLOAT64, params, nrows, mode);
        case SType::INT64:   return resolve1<double, int64_t, double>(opcode, SType::FLOAT64, params, nrows, mode);
        case SType::FLOAT32: return resolve1<double, float, double>(opcode, SType::FLOAT64, params, nrows, mode);
        case SType::FLOAT64: return resolve1<double, double, double>(opcode, SType::FLOAT64, params, nrows, mode);
        default: break;
      }
      break;

    case SType::STR32:
      switch (rhs_type) {
        case SType::STR32: return resolve1str<uint32_t, uint32_t>(opcode, params, nrows, mode);
        case SType::STR64: return resolve1str<uint32_t, uint64_t>(opcode, params, nrows, mode);
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

Column* binaryop(int opcode, Column* lhs, Column* rhs)
{
  lhs->reify();
  rhs->reify();
  int64_t lhs_nrows = lhs->nrows;
  int64_t rhs_nrows = rhs->nrows;
  SType lhs_type = lhs->stype();
  SType rhs_type = rhs->stype();
  void* params[3];
  params[0] = lhs;
  params[1] = rhs;
  params[2] = nullptr;

  mapperfn mapfn = nullptr;
  mapfn = resolve0(lhs_type, rhs_type, opcode, params, lhs_nrows,
                   lhs_nrows == rhs_nrows? OpMode::N_to_N :
                   rhs_nrows == 1? OpMode::N_to_One :
                   lhs_nrows == 1? OpMode::One_to_N : OpMode::Error);
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

};  // namespace expr
