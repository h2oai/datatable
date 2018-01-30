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
#include "types.h"

namespace expr
{


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



//------------------------------------------------------------------------------
// Resolve template function
//------------------------------------------------------------------------------

static mapperfn resolve0(SType stype) {
  switch (stype) {
    case ST_BOOLEAN_I1:
    case ST_INTEGER_I1:  return mean_skipna<int8_t, double>;
    case ST_INTEGER_I2:  return mean_skipna<int16_t, double>;
    case ST_INTEGER_I4:  return mean_skipna<int32_t, double>;
    case ST_INTEGER_I8:  return mean_skipna<int64_t, double>;
    case ST_REAL_F4:     return mean_skipna<float, float>;
    case ST_REAL_F8:     return mean_skipna<double, double>;
    default:             return nullptr;
  }
}


Column* mean(Column* arg)
{
  SType arg_type = arg->stype();
  void* params[2];
  params[0] = arg;
  params[1] = Column::new_data_column(ST_REAL_F8, 1);

  mapperfn fn = resolve0(arg_type);
  if (!fn) {
    throw RuntimeError()
      << "Unable to apply mean function to column(stype=" << arg_type << ")";
  }

  (*fn)(0, arg->nrows, params);

  return static_cast<Column*>(params[1]);
}

};  // namespace expr
