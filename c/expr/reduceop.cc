//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <cmath>             // std::sqrt
#include <limits>            // std::numeric_limits<?>::max, ::infinity
#include <type_traits>
#include <unordered_map>
#include "expr/base_expr.h"  // ReduceOp
#include "expr/py_expr.h"
#include "utils/parallel.h"
#include "datatablemodule.h"
#include "types.h"

namespace expr
{


template<typename T>
constexpr T infinity() {
  return std::numeric_limits<T>::has_infinity
         ? std::numeric_limits<T>::infinity()
         : std::numeric_limits<T>::max();
}


//------------------------------------------------------------------------------
// Reducer library
//------------------------------------------------------------------------------

struct Reducer {
  using reducer_fn = void (*)(const RowIndex& ri, size_t row0, size_t row1,
                              const void* input, void* output, size_t grp);

  reducer_fn f;
  SType output_stype;
  size_t : 56;
};


class ReducerLibrary {
  private:
    std::unordered_map<size_t, Reducer> reducers;

  public:
    void add(ReduceOp op, Reducer::reducer_fn f, SType inp_stype,
             SType out_stype)
    {
      size_t id = key(op, inp_stype);
      xassert(reducers.count(id) == 0);
      reducers[id] = Reducer {f, out_stype};
    }

    const Reducer* lookup(ReduceOp op, SType stype) const {
      size_t id = key(op, stype);
      if (reducers.count(id) == 0) return nullptr;
      return &(reducers.at(id));
    }

  private:
    constexpr inline size_t key(ReduceOp op, SType stype) const {
      return static_cast<size_t>(op) +
             REDUCEOP_COUNT * static_cast<size_t>(stype);
    }
};


static ReducerLibrary library;




//------------------------------------------------------------------------------
// "First" reducer
//------------------------------------------------------------------------------

Column* reduce_first(const Column* col, const Groupby& groupby) {
  if (col->nrows == 0) {
    return Column::new_data_column(col->stype(), 0);
  }
  size_t ngrps = groupby.ngroups();
  // groupby.offsets array has length `ngrps + 1` and contains offsets of the
  // beginning of each group. We will take this array and reinterpret it as a
  // RowIndex (taking only the first `ngrps` elements). Applying this rowindex
  // to the column will produce the vector of first elements in that column.
  arr32_t indices(ngrps, groupby.offsets_r());
  RowIndex ri = RowIndex(std::move(indices), true)
                * col->rowindex();
  Column* res = col->shallowcopy(ri);
  if (ngrps == 1) res->materialize();
  return res;
}



//------------------------------------------------------------------------------
// Sum calculation
//------------------------------------------------------------------------------

template<typename T, typename U>
static void sum_reducer(const RowIndex& ri, size_t row0, size_t row1,
                        const void* inp, void* out, size_t grp_index)
{
  const T* inputs = static_cast<const T*>(inp);
  U* outputs = static_cast<U*>(out);
  U sum = 0;
  ri.iterate(row0, row1, 1,
    [&](size_t, size_t j) {
      if (j == RowIndex::NA) return;
      T x = inputs[j];
      if (!ISNA<T>(x))
        sum += static_cast<U>(x);
    });
  outputs[grp_index] = sum;
}



//------------------------------------------------------------------------------
// Count calculation
//------------------------------------------------------------------------------

template<typename T>
static void count_reducer(const RowIndex& ri, size_t row0, size_t row1,
                          const void* inp, void* out, size_t grp_index)
{
  const T* inputs = static_cast<const T*>(inp);
  int64_t count = 0;
  ri.iterate(row0, row1, 1,
    [&](size_t, size_t j) {
      if (j == RowIndex::NA) return;
      count += !ISNA<T>(inputs[j]);
    });
  int64_t* outputs = static_cast<int64_t*>(out);
  outputs[grp_index] = count;
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
  size_t row0 = static_cast<size_t>(groups[grp]);
  size_t row1 = static_cast<size_t>(groups[grp + 1]);
  col0->rowindex().iterate(row0, row1, 1,
    [&](size_t, size_t j) {
      if (j == RowIndex::NA) return;
      IT x = inputs[j];
      if (ISNA<IT>(x)) return;
      OT y = static_cast<OT>(x) - delta;
      OT t = sum + y;
      delta = (t - sum) - y;
      sum = t;
      cnt++;
    });
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
  size_t row0 = static_cast<size_t>(groups[grp]);
  size_t row1 = static_cast<size_t>(groups[grp + 1]);
  col0->rowindex().iterate(row0, row1, 1,
    [&](size_t, size_t j) {
      if (j == RowIndex::NA) return;
      IT x = inputs[j];
      if (ISNA<IT>(x)) return;
      cnt++;
      OT t1 = x - mean;
      mean += t1 / cnt;
      OT t2 = x - mean;
      m2 += t1 * t2;
    });
  outputs[grp] = cnt <= 1? GETNA<OT>() : std::sqrt(m2 / (cnt - 1));
}



//------------------------------------------------------------------------------
// Minimum
//------------------------------------------------------------------------------

template<typename T>
static void min_reducer(const RowIndex& ri, size_t row0, size_t row1,
                        const void* inp, void* out, size_t grp_index)
{
  const T* inputs = static_cast<const T*>(inp);
  T* outputs = static_cast<T*>(out);
  T res = infinity<T>();
  bool valid = false;
  ri.iterate(row0, row1, 1,
    [&](size_t, size_t j) {
      if (j == RowIndex::NA) return;
      T x = inputs[j];
      if (ISNA<T>(x)) return;
      if (x < res) res = x;
      valid = true;
    });
  outputs[grp_index] = valid? res : GETNA<T>();
}



//------------------------------------------------------------------------------
// Maximum
//------------------------------------------------------------------------------

template<typename T>
static void max_reducer(const RowIndex& ri, size_t row0, size_t row1,
                        const void* inp, void* out, size_t grp_index)
{
  const T* inputs = static_cast<const T*>(inp);
  T* outputs = static_cast<T*>(out);
  T res = -infinity<T>();
  bool valid = false;
  ri.iterate(row0, row1, 1,
    [&](size_t, size_t j) {
      if (j == RowIndex::NA) return;
      T x = inputs[j];
      if (ISNA<T>(x)) return;
      if (x > res) res = x;
      valid = true;
    });
  outputs[grp_index] = valid? res : GETNA<T>();
}



//------------------------------------------------------------------------------
// Resolve template function
//------------------------------------------------------------------------------

template<typename T1, typename T2>
static gmapperfn resolve1(ReduceOp opcode) {
  switch (opcode) {
    case ReduceOp::MEAN:  return mean_skipna<T1, T2>;
    case ReduceOp::STDEV: return stdev_skipna<T1, T2>;
    default:            return nullptr;
  }
}


static gmapperfn resolve0(ReduceOp opcode, SType stype) {

  switch (stype) {
    case SType::BOOL:
    case SType::INT8:    return resolve1<int8_t, double>(opcode);
    case SType::INT16:   return resolve1<int16_t, double>(opcode);
    case SType::INT32:   return resolve1<int32_t, double>(opcode);
    case SType::INT64:   return resolve1<int64_t, double>(opcode);
    case SType::FLOAT32: return resolve1<float, float>(opcode);
    case SType::FLOAT64: return resolve1<double, double>(opcode);
    default:             return nullptr;
  }
}



//------------------------------------------------------------------------------
// External API
//------------------------------------------------------------------------------

Column* reduceop(ReduceOp opcode, Column* arg, const Groupby& groupby)
{
  if (opcode == ReduceOp::FIRST) {
    return reduce_first(arg, groupby);
  }
  SType arg_type = arg->stype();

  auto preducer = library.lookup(opcode, arg_type);
  if (preducer) {
    SType out_stype = preducer->output_stype;
    size_t out_nrows = groupby.ngroups();
    if (out_nrows == 0) out_nrows = 1;
    Column* res = Column::new_data_column(out_stype, out_nrows);
    const RowIndex& rowindex = arg->rowindex();
    const void* input = arg->data();
    if (arg_type == SType::STR32) input = static_cast<const char*>(input) + 4;
    if (arg_type == SType::STR64) input = static_cast<const char*>(input) + 8;
    void* output = res->data_w();

    if (out_nrows == 1) {
      preducer->f(rowindex, 0, arg->nrows, input, output, 0);
    } else {
      const int32_t* groups = groupby.offsets_r();

      #pragma omp parallel for schedule(static)
      for (size_t i = 0; i < out_nrows; ++i) {
        size_t row0 = static_cast<size_t>(groups[i]);
        size_t row1 = static_cast<size_t>(groups[i + 1]);
        preducer->f(rowindex, row0, row1, input, output, i);
      }
    }
    return res;
  }


  SType res_type = arg_type == SType::FLOAT32 ? arg_type : SType::FLOAT64;

  int32_t ngrps = static_cast<int32_t>(groupby.ngroups());
  if (ngrps == 0) ngrps = 1;

  void* params[2];
  params[0] = arg;
  params[1] = Column::new_data_column(res_type, static_cast<size_t>(ngrps));

  gmapperfn fn = resolve0(opcode, arg_type);
  if (!fn) {
    throw RuntimeError()
      << "Unable to apply reduce function " << static_cast<size_t>(opcode)
      << " to column of type `" << arg_type << "`";
  }

  int32_t _grps[2] = {0, static_cast<int32_t>(arg->nrows)};
  const int32_t* grps = ngrps == 1? _grps : groupby.offsets_r();

  #pragma omp parallel for schedule(static)
  for (int32_t g = 0; g < ngrps; ++g) {
    (*fn)(grps, g, params);
  }

  return static_cast<Column*>(params[1]);
}

};  // namespace expr



//------------------------------------------------------------------------------
// Initialization
//------------------------------------------------------------------------------

void py::DatatableModule::init_reducers()
{
  using namespace expr;

  // Count
  library.add(ReduceOp::COUNT, count_reducer<int8_t>,   SType::BOOL, SType::INT64);
  library.add(ReduceOp::COUNT, count_reducer<int8_t>,   SType::INT8, SType::INT64);
  library.add(ReduceOp::COUNT, count_reducer<int16_t>,  SType::INT16, SType::INT64);
  library.add(ReduceOp::COUNT, count_reducer<int32_t>,  SType::INT32, SType::INT64);
  library.add(ReduceOp::COUNT, count_reducer<int64_t>,  SType::INT64, SType::INT64);
  library.add(ReduceOp::COUNT, count_reducer<float>,    SType::FLOAT32, SType::INT64);
  library.add(ReduceOp::COUNT, count_reducer<double>,   SType::FLOAT64, SType::INT64);
  library.add(ReduceOp::COUNT, count_reducer<uint32_t>, SType::STR32, SType::INT64);
  library.add(ReduceOp::COUNT, count_reducer<uint64_t>, SType::STR64, SType::INT64);

  // Min
  library.add(ReduceOp::MIN, min_reducer<int8_t>,  SType::BOOL, SType::BOOL);
  library.add(ReduceOp::MIN, min_reducer<int8_t>,  SType::INT8, SType::INT8);
  library.add(ReduceOp::MIN, min_reducer<int16_t>, SType::INT16, SType::INT16);
  library.add(ReduceOp::MIN, min_reducer<int32_t>, SType::INT32, SType::INT32);
  library.add(ReduceOp::MIN, min_reducer<int64_t>, SType::INT64, SType::INT64);
  library.add(ReduceOp::MIN, min_reducer<float>,   SType::FLOAT32, SType::FLOAT32);
  library.add(ReduceOp::MIN, min_reducer<double>,  SType::FLOAT64, SType::FLOAT64);

  // Max
  library.add(ReduceOp::MAX, max_reducer<int8_t>,  SType::BOOL, SType::BOOL);
  library.add(ReduceOp::MAX, max_reducer<int8_t>,  SType::INT8, SType::INT8);
  library.add(ReduceOp::MAX, max_reducer<int16_t>, SType::INT16, SType::INT16);
  library.add(ReduceOp::MAX, max_reducer<int32_t>, SType::INT32, SType::INT32);
  library.add(ReduceOp::MAX, max_reducer<int64_t>, SType::INT64, SType::INT64);
  library.add(ReduceOp::MAX, max_reducer<float>,   SType::FLOAT32, SType::FLOAT32);
  library.add(ReduceOp::MAX, max_reducer<double>,  SType::FLOAT64, SType::FLOAT64);

  // Sum
  library.add(ReduceOp::SUM, sum_reducer<int8_t,  int64_t>,  SType::BOOL, SType::INT64);
  library.add(ReduceOp::SUM, sum_reducer<int8_t,  int64_t>,  SType::INT8, SType::INT64);
  library.add(ReduceOp::SUM, sum_reducer<int16_t, int64_t>,  SType::INT16, SType::INT64);
  library.add(ReduceOp::SUM, sum_reducer<int32_t, int64_t>,  SType::INT32, SType::INT64);
  library.add(ReduceOp::SUM, sum_reducer<int64_t, int64_t>,  SType::INT64, SType::INT64);
  library.add(ReduceOp::SUM, sum_reducer<float,   double>,   SType::FLOAT32, SType::FLOAT64);
  library.add(ReduceOp::SUM, sum_reducer<double,  double>,   SType::FLOAT64, SType::FLOAT64);
}
