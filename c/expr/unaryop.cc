//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "expr/py_expr.h"
#include "types.h"

namespace expr
{

enum OpCode {
  IsNa   = 1,
  Minus  = 2,
  Plus   = 3,
  Invert = 4,
};


//------------------------------------------------------------------------------
// Final mapper function
//------------------------------------------------------------------------------

template<typename IT, typename OT, OT (*OP)(IT)>
static void map_n(int64_t row0, int64_t row1, void** params) {
  IT* arg_data = static_cast<IT*>(static_cast<Column*>(params[0])->data());
  OT* res_data = static_cast<OT*>(static_cast<Column*>(params[1])->data());
  for (int64_t i = row0; i < row1; ++i) {
    res_data[i] = OP(arg_data[i]);
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




//------------------------------------------------------------------------------
// Method resolution
//------------------------------------------------------------------------------

template<typename IT>
static mapperfn resolve1(int opcode) {
  switch (opcode) {
    case IsNa:    return map_n<IT, int8_t, op_isna<IT>>;
    case Minus:   return map_n<IT, IT, op_minus<IT>>;
    case Invert:
      if (std::is_floating_point<IT>::value) return nullptr;
      return map_n<IT, IT, Inverse<IT>::impl>;
  }
}


static mapperfn resolve0(SType stype, int opcode) {
  switch (stype) {
    case ST_BOOLEAN_I1: return resolve1<int8_t>(opcode);
    case ST_INTEGER_I1: return resolve1<int8_t>(opcode);
    case ST_INTEGER_I2: return resolve1<int16_t>(opcode);
    case ST_INTEGER_I4: return resolve1<int32_t>(opcode);
    case ST_INTEGER_I8: return resolve1<int64_t>(opcode);
    case ST_REAL_F4:    return resolve1<float>(opcode);
    case ST_REAL_F8:    return resolve1<double>(opcode);
    default:            return nullptr;
  }
}


Column* unaryop(int opcode, Column* arg)
{
  if (opcode == OpCode::Plus) return arg->shallowcopy();

  SType arg_type = arg->stype();
  SType res_type = arg_type;
  if (opcode == OpCode::IsNa) res_type = ST_BOOLEAN_I1;
  else if (arg_type == ST_BOOLEAN_I1) res_type = ST_INTEGER_I1;
  void* params[2];
  params[0] = arg;
  params[1] = Column::new_data_column(res_type, arg->nrows);

  mapperfn fn = resolve0(arg_type, opcode);
  if (!fn) {
    throw RuntimeError()
      << "Unable to apply unary op " << opcode << " to column(stype="
      << arg_type << ")";
  }

  (*fn)(0, arg->nrows, params);

  return static_cast<Column*>(params[1]);
}


};  // namespace expr
