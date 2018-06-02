//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "expr/py_expr.h"
#include <cmath>      // std::sqrt
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
  First = 5,
};

template<typename T>
constexpr T infinity() {
  return std::numeric_limits<T>::has_infinity
         ? std::numeric_limits<T>::infinity()
         : std::numeric_limits<T>::max();
}


//------------------------------------------------------------------------------
// "First" reducer
//------------------------------------------------------------------------------

static Column* reduce_first(Column* arg, const Groupby& groupby) {
  if (arg->nrows == 0) {
    return Column::new_data_column(arg->stype(), 0);
  }
  size_t ngrps = groupby.ngroups();
  arr32_t indices(ngrps);
  // TODO: avoid copy (by allowing RowIndex to be created from a MemoryRange)
  std::memcpy(indices.data(), groupby.offsets_r(), ngrps * sizeof(int32_t));
  RowIndex ri = RowIndex::from_array32(std::move(indices), true);
  Column* res = arg->shallowcopy(ri);
  res->reify();
  return res;
}




//------------------------------------------------------------------------------
// Mean calculation
//------------------------------------------------------------------------------

template<typename IT, typename OT>
static void mean_skipna(const int32_t* groups, int32_t grp, void** params) {
  Column* col0 = static_cast<Column*>(params[0]);
  Column* col1 = static_cast<Column*>(params[1]);
  const IT* inputs = static_cast<const IT*>(col0->data());
  OT* outputs = static_cast<OT*>(col1->data_w());
  OT sum = 0;
  int64_t cnt = 0;
  OT delta = 0;
  int32_t row0 = groups[grp];
  int32_t row1 = groups[grp + 1];
  for (int32_t i = row0; i < row1; ++i) {
    IT x = inputs[i];
    if (ISNA<IT>(x)) continue;
    OT y = static_cast<OT>(x) - delta;
    OT t = sum + y;
    delta = (t - sum) - y;
    sum = t;
    cnt++;
  }
  outputs[grp] = cnt == 0? GETNA<OT>() : sum / cnt;
}



//------------------------------------------------------------------------------
// Standard deviation
//------------------------------------------------------------------------------

// Welford algorithm
template<typename IT, typename OT>
static void stdev_skipna(const int32_t* groups, int32_t grp, void** params) {
  Column* col0 = static_cast<Column*>(params[0]);
  Column* col1 = static_cast<Column*>(params[1]);
  const IT* inputs = static_cast<const IT*>(col0->data());
  OT* outputs = static_cast<OT*>(col1->data_w());
  OT mean = 0;
  OT m2 = 0;
  int64_t cnt = 0;
  int32_t row0 = groups[grp];
  int32_t row1 = groups[grp + 1];
  for (int32_t i = row0; i < row1; ++i) {
    IT x = inputs[i];
    if (ISNA<IT>(x)) continue;
    cnt++;
    OT t1 = x - mean;
    mean += t1 / cnt;
    OT t2 = x - mean;
    m2 += t1 * t2;
  }
  outputs[grp] = cnt <= 1? GETNA<OT>() : std::sqrt(m2 / (cnt - 1));
}



//------------------------------------------------------------------------------
// Minimum
//------------------------------------------------------------------------------

template<typename T>
static void min_skipna(const int32_t* groups, int32_t grp, void** params) {
  Column* col0 = static_cast<Column*>(params[0]);
  Column* col1 = static_cast<Column*>(params[1]);
  const T* inputs = static_cast<const T*>(col0->data());
  T* outputs = static_cast<T*>(col1->data_w());
  T res = infinity<T>();
  int32_t row0 = groups[grp];
  int32_t row1 = groups[grp + 1];
  for (int32_t i = row0; i < row1; ++i) {
    T x = inputs[i];
    if (!ISNA<T>(x) && x < res) {
      res = x;
    }
  }
  outputs[grp] = res;
}



//------------------------------------------------------------------------------
// Maximum
//------------------------------------------------------------------------------

template<typename T>
static void max_skipna(const int32_t* groups, int32_t grp, void** params) {
  Column* col0 = static_cast<Column*>(params[0]);
  Column* col1 = static_cast<Column*>(params[1]);
  const T* inputs = static_cast<const T*>(col0->data());
  T* outputs = static_cast<T*>(col1->data_w());
  T res = -infinity<T>();
  int32_t row0 = groups[grp];
  int32_t row1 = groups[grp + 1];
  for (int32_t i = row0; i < row1; ++i) {
    T x = inputs[i];
    if (!ISNA<T>(x) && x > res) {
      res = x;
    }
  }
  outputs[grp] = res;
}



//------------------------------------------------------------------------------
// Resolve template function
//------------------------------------------------------------------------------

template<typename T1, typename T2>
static gmapperfn resolve1(int opcode) {
  switch (opcode) {
    case OpCode::Mean:  return mean_skipna<T1, T2>;
    case OpCode::Min:   return min_skipna<T1>;
    case OpCode::Max:   return max_skipna<T1>;
    case OpCode::Stdev: return stdev_skipna<T1, T2>;
    default:            return nullptr;
  }
}


static gmapperfn resolve0(int opcode, SType stype) {
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

Column* reduceop(int opcode, Column* arg, const Groupby& groupby)
{
  if (opcode == OpCode::First) {
    return reduce_first(arg, groupby);
  }
  SType arg_type = arg->stype();
  SType res_type = opcode == OpCode::Min || opcode == OpCode::Max ||
                   arg_type == ST_REAL_F4 ? arg_type : ST_REAL_F8;
  int32_t ngrps = static_cast<int32_t>(groupby.ngroups());
  if (ngrps == 0) ngrps = 1;

  void* params[2];
  params[0] = arg;
  params[1] = Column::new_data_column(res_type, ngrps);

  gmapperfn fn = resolve0(opcode, arg_type);
  if (!fn) {
    throw RuntimeError()
      << "Unable to apply reduce function " << opcode
      << " to column(stype=" << arg_type << ")";
  }

  int32_t _grps[2] = {0, static_cast<int32_t>(arg->nrows)};
  const int32_t* grps = ngrps == 1? _grps : groupby.offsets_r();
  for (int32_t g = 0; g < ngrps; ++g) {
    (*fn)(grps, g, params);
  }

  return static_cast<Column*>(params[1]);
}

};  // namespace expr
