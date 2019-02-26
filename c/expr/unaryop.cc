//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "expr/base_expr.h"
#include "expr/py_expr.h"
#include "types.h"

namespace expr
{


//------------------------------------------------------------------------------
// Final mapper function
//------------------------------------------------------------------------------

template<typename IT, typename OT, OT (*OP)(IT)>
static void map_n(int64_t row0, int64_t row1, void** params) {
  Column* col0 = static_cast<Column*>(params[0]);
  Column* col1 = static_cast<Column*>(params[1]);
  const IT* arg_data = static_cast<const IT*>(col0->data());
  OT* res_data = static_cast<OT*>(col1->data_w());
  for (int64_t i = row0; i < row1; ++i) {
    res_data[i] = OP(arg_data[i]);
  }
}

template<typename IT, typename OT, OT (*OP)(IT, IT)>
static void strmap_n(int64_t row0, int64_t row1, void** params) {
  StringColumn<IT>* col0 = static_cast<StringColumn<IT>*>(params[0]);
  Column* col1 = static_cast<Column*>(params[1]);
  const IT* arg_data = col0->offsets();
  OT* res_data = static_cast<OT*>(col1->data_w());
  for (int64_t i = row0; i < row1; ++i) {
    res_data[i] = OP(arg_data[i - 1] & ~GETNA<IT>(), arg_data[i]);
  }
}


//------------------------------------------------------------------------------
// Operator implementations
//------------------------------------------------------------------------------

template<typename T>
inline static T op_minus(T x) {
  return IsIntNA<T>(x) ? x : -x;
}

template<typename T>
inline static int8_t op_isna(T x) {
  return ISNA<T>(x);
}

template <typename T>
inline static T op_abs(T x) {
  // If T is floating point and x is NA, then (x < 0) will evaluate to false;
  // If T is integer and x is NA, then (x < 0) will be true, but -x will be
  // equal to x.
  // Thus, in all cases we'll have `abs(NA) == NA`.
  return (x < 0) ? -x : x;
}

template <typename T>
inline static double op_exp(T x) {
  return ISNA<T>(x) ? GETNA<double>() : std::exp(static_cast<double>(x));
}


template <typename T>
inline static double op_loge(T x) {
  return ISNA<T>(x) || x < 0 ? GETNA<double>()
                             : std::log(static_cast<double>(x));
}


template <typename T>
inline static double op_log10(T x) {
  return ISNA<T>(x) || x < 0 ? GETNA<double>()
                             : std::log10(static_cast<double>(x));
}


template<typename T>
struct Inverse {
  inline static T impl(T x) {
    return ISNA<T>(x) ? x : ~x;
  }
};

template<>
struct Inverse<float> {
  inline static float impl(float) { return 0; }
};

template<>
struct Inverse<double> {
  inline static double impl(double) { return 0; }
};

inline static int8_t bool_inverse(int8_t x) {
  return ISNA<int8_t>(x)? x : !x;
}



//------------------------------------------------------------------------------
// String operators
//------------------------------------------------------------------------------

template <typename T>
inline static int8_t op_isna_str(T, T end) {
  return ISNA<T>(end);
}

template <typename IT, typename OT>
inline static OT op_len_str(IT start, IT end) {
  return ISNA<IT>(end)? GETNA<OT>() : static_cast<OT>(end - start);
}



//------------------------------------------------------------------------------
// Method resolution
//------------------------------------------------------------------------------

template<typename IT>
static mapperfn resolve1(dt::unop opcode) {
  switch (opcode) {
    case dt::unop::ISNA:    return map_n<IT, int8_t, op_isna<IT>>;
    case dt::unop::MINUS:   return map_n<IT, IT, op_minus<IT>>;
    case dt::unop::ABS:     return map_n<IT, IT, op_abs<IT>>;
    case dt::unop::EXP:     return map_n<IT, double, op_exp<IT>>;
    case dt::unop::LOGE:    return map_n<IT, double, op_loge<IT>>;
    case dt::unop::LOG10:   return map_n<IT, double, op_log10<IT>>;
    case dt::unop::INVERT:
      if (std::is_floating_point<IT>::value) return nullptr;
      return map_n<IT, IT, Inverse<IT>::impl>;
    default:                return nullptr;
  }
}


template<typename T>
static mapperfn resolve_str(dt::unop opcode) {
  using OT = typename std::make_signed<T>::type;
  switch (opcode) {
    case dt::unop::ISNA: return strmap_n<T, int8_t, op_isna_str<T>>;
    case dt::unop::LEN:  return strmap_n<T, OT, op_len_str<T, OT>>;
    default:             return nullptr;
  }
}


static mapperfn resolve0(SType stype, dt::unop opcode) {
  switch (stype) {
    case SType::BOOL:
      if (opcode == dt::unop::INVERT) return map_n<int8_t, int8_t, bool_inverse>;
      return resolve1<int8_t>(opcode);
    case SType::INT8:    return resolve1<int8_t>(opcode);
    case SType::INT16:   return resolve1<int16_t>(opcode);
    case SType::INT32:   return resolve1<int32_t>(opcode);
    case SType::INT64:   return resolve1<int64_t>(opcode);
    case SType::FLOAT32: return resolve1<float>(opcode);
    case SType::FLOAT64: return resolve1<double>(opcode);
    case SType::STR32:   return resolve_str<uint32_t>(opcode);
    case SType::STR64:   return resolve_str<uint64_t>(opcode);
    default: break;
  }
  return nullptr;
}


Column* unaryop(dt::unop opcode, Column* arg)
{
  if (opcode == dt::unop::PLUS) return arg->shallowcopy();
  arg->reify();

  SType arg_type = arg->stype();
  SType res_type = arg_type;
  if (opcode == dt::unop::ISNA) {
    res_type = SType::BOOL;
  } else if (arg_type == SType::BOOL && opcode == dt::unop::MINUS) {
    res_type = SType::INT8;
  } else if (opcode == dt::unop::EXP || opcode == dt::unop::LOGE ||
             opcode == dt::unop::LOG10) {
    res_type = SType::FLOAT64;
  } else if (opcode == dt::unop::LEN) {
    res_type = arg_type == SType::STR32? SType::INT32 : SType::INT64;
  }
  void* params[2];
  params[0] = arg;
  params[1] = Column::new_data_column(res_type, arg->nrows);

  mapperfn fn = resolve0(arg_type, opcode);
  if (!fn) {
    throw RuntimeError()
      << "Unable to apply unary op " << int(opcode) << " to column(stype="
      << arg_type << ")";
  }

  (*fn)(0, static_cast<int64_t>(arg->nrows), params);

  return static_cast<Column*>(params[1]);
}


};  // namespace expr
