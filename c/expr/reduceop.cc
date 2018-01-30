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
#include "expr/py_expr.h"
#include <limits>     // std::numeric_limits<?>::max, ::infinity
#include "types.h"

namespace expr
{

// Synchronize with expr/consts.py
enum OpCode {
  Mean  = 1,
  Min   = 2,
  Max   = 3,
  Stdev = 4,
};

template<typename T>
constexpr T infinity() {
  return std::numeric_limits<T>::has_infinity
         ? std::numeric_limits<T>::infinity()
         : std::numeric_limits<T>::max();
}



//------------------------------------------------------------------------------
// Final reducer function
//------------------------------------------------------------------------------

// Using Kahan summation algorithm
template<typename IT, typename OT>
static void mean_skipna(int64_t row0, int64_t row1, void** params) {
  IT* inputs = static_cast<IT*>(static_cast<Column*>(params[0])->data());
  OT* output = static_cast<OT*>(static_cast<Column*>(params[1])->data());
  OT sum = 0;
  int64_t cnt = 0;
  OT delta = 0;
  for (int64_t i = row0; i < row1; ++i) {
    IT x = inputs[i];
    if (ISNA<IT>(x)) continue;
    OT y = static_cast<OT>(x) - delta;
    OT t = sum + y;
    delta = (t - sum) - y;
    sum = t;
    cnt++;
  }
  output[0] = cnt == 0? GETNA<OT>() : sum / cnt;
}

template<typename T>
static void min_skipna(int64_t row0, int64_t row1, void** params) {
  T* inputs = static_cast<T*>(static_cast<Column*>(params[0])->data());
  T* output = static_cast<T*>(static_cast<Column*>(params[1])->data());
  T res = infinity<T>();
  for (int64_t i = row0; i < row1; ++i) {
    T x = inputs[i];
    if (!ISNA<T>(x) && x < res) {
      res = x;
    }
  }
  output[0] = res;
}

template<typename T>
static void max_skipna(int64_t row0, int64_t row1, void** params) {
  T* inputs = static_cast<T*>(static_cast<Column*>(params[0])->data());
  T* output = static_cast<T*>(static_cast<Column*>(params[1])->data());
  T res = -infinity<T>();
  for (int64_t i = row0; i < row1; ++i) {
    T x = inputs[i];
    if (!ISNA<T>(x) && x > res) {
      res = x;
    }
  }
  output[0] = res;
}



//------------------------------------------------------------------------------
// Resolve template function
//------------------------------------------------------------------------------

template<typename T1, typename T2>
static mapperfn resolve1(int opcode) {
  switch (opcode) {
    case OpCode::Mean:  return mean_skipna<T1, T2>;
    case OpCode::Min:   return min_skipna<T1>;
    case OpCode::Max:   return min_skipna<T1>;
    // case OpCode::Stdev: return stdev_skipna<T1, T2>;
    default:            return nullptr;
  }
}


static mapperfn resolve0(int opcode, SType stype) {
  switch (stype) {
    case ST_BOOLEAN_I1:
    case ST_INTEGER_I1:  return resolve1<int8_t, double>(opcode);
    case ST_INTEGER_I2:  return resolve1<int16_t, double>(opcode);
    case ST_INTEGER_I4:  return resolve1<int32_t, double>(opcode);
    case ST_INTEGER_I8:  return resolve1<int64_t, double>(opcode);
    case ST_REAL_F4:     return resolve1<float, float>(opcode);
    case ST_REAL_F8:     return resolve1<double, double>(opcode);
    default:             return nullptr;
  }
}



//------------------------------------------------------------------------------
// External API
//------------------------------------------------------------------------------

Column* reduceop(int opcode, Column* arg)
{
  SType arg_type = arg->stype();
  SType res_type = opcode == OpCode::Min || opcode == OpCode::Max ||
                   arg_type == ST_REAL_F4 ? arg_type : ST_REAL_F8;
  void* params[2];
  params[0] = arg;
  params[1] = Column::new_data_column(res_type, 1);

  mapperfn fn = resolve0(opcode, arg_type);
  if (!fn) {
    throw RuntimeError()
      << "Unable to apply reduce function " << opcode
      << " to column(stype=" << arg_type << ")";
  }

  (*fn)(0, arg->nrows, params);

  return static_cast<Column*>(params[1]);
}

};  // namespace expr
