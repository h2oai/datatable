//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <cmath>             // std::sqrt
#include <limits>            // std::numeric_limits<?>::max, ::infinity
#include <memory>            // std::unique_ptr
#include <unordered_map>     // std::unordered_map
#include "expr/base_expr.h"  // ReduceOp
#include "utils/parallel.h"
#include "types.h"
namespace expr {


template<typename T>
constexpr T infinity() {
  return std::numeric_limits<T>::has_infinity
         ? std::numeric_limits<T>::infinity()
         : std::numeric_limits<T>::max();
}

using colptr = std::unique_ptr<Column>;



//------------------------------------------------------------------------------
// Reducer library
//------------------------------------------------------------------------------
using reducer_fn = void (*)(const RowIndex& ri, size_t row0, size_t row1,
                            const void* input, void* output, size_t grp);

struct Reducer {
  reducer_fn f;
  SType output_stype;
  size_t : 56;
};


class ReducerLibrary {
  private:
    std::unordered_map<size_t, Reducer> reducers;

  public:
    void add(ReduceOp op, reducer_fn f, SType inp_stype, SType out_stype) {
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
    static constexpr size_t key(ReduceOp op, SType stype) {
      return static_cast<size_t>(op) +
             REDUCEOP_COUNT * static_cast<size_t>(stype);
    }
};


static ReducerLibrary library;




//------------------------------------------------------------------------------
// "First" reducer
//------------------------------------------------------------------------------

static colptr reduce_first(const colptr& col, const Groupby& groupby)
{
  if (col->nrows == 0) {
    return colptr(Column::new_data_column(col->stype(), 0));
  }
  size_t ngrps = groupby.ngroups();
  // groupby.offsets array has length `ngrps + 1` and contains offsets of the
  // beginning of each group. We will take this array and reinterpret it as a
  // RowIndex (taking only the first `ngrps` elements). Applying this rowindex
  // to the column will produce the vector of first elements in that column.
  arr32_t indices(ngrps, groupby.offsets_r());
  RowIndex ri = RowIndex(std::move(indices), true)
                * col->rowindex();
  auto res = colptr(col->shallowcopy(ri));
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

template<typename T, typename U>
static void mean_reducer(const RowIndex& ri, size_t row0, size_t row1,
                        const void* inp, void* out, size_t grp_index)
{
  const T* inputs = static_cast<const T*>(inp);
  U* outputs = static_cast<U*>(out);
  U sum = 0;
  int64_t count = 0;
  ri.iterate(row0, row1, 1,
    [&](size_t, size_t j) {
      if (j == RowIndex::NA) return;
      T x = inputs[j];
      if (!ISNA<T>(x)) {
        sum += static_cast<U>(x);
        count++;
      }
    });
  outputs[grp_index] = (count == 0)? GETNA<U>() : sum / count;
}



//------------------------------------------------------------------------------
// Standard deviation
//------------------------------------------------------------------------------

// Welford algorithm
template<typename T, typename U>
static void stdev_reducer(const RowIndex& ri, size_t row0, size_t row1,
                          const void* inp, void* out, size_t grp_index)
{
  const T* inputs = static_cast<const T*>(inp);
  U* outputs = static_cast<U*>(out);
  U mean = 0;
  U m2 = 0;
  int64_t count = 0;
  ri.iterate(row0, row1, 1,
    [&](size_t, size_t j) {
      if (j == RowIndex::NA) return;
      T x = inputs[j];
      if (!ISNA<T>(x)) {
        count++;
        U tmp1 = static_cast<U>(x) - mean;
        mean += tmp1 / count;
        U tmp2 = static_cast<U>(x) - mean;
        m2 += tmp1 * tmp2;
      }
    });
  outputs[grp_index] = (count <= 1)? GETNA<U>() : std::sqrt(m2/(count - 1));
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
// Median
//------------------------------------------------------------------------------

template<typename T, typename U>
static void median_reducer(const RowIndex& ri, size_t row0, size_t row1,
                           const void* inp, void* out, size_t grp_index)
{
  const T* inputs = static_cast<const T*>(inp);
  U* outputs = static_cast<U*>(out);

  // skip NA values if any
  for (; row0 < row1; ++row0) {
    size_t j = ri[row0];
    if (j != RowIndex::NA && !ISNA<T>(inputs[j])) break;
  }

  if (row0 == row1) {
    outputs[grp_index] = GETNA<U>();
  } else {
    size_t j = (row1 + row0) / 2;
    outputs[grp_index] = ((row1 - row0) & 1)
      ? static_cast<U>(inputs[ri[j]]) :
        (static_cast<U>(inputs[ri[j]]) + static_cast<U>(inputs[ri[j - 1]]))/2;
  }
}



//------------------------------------------------------------------------------
// expr_reduce
//------------------------------------------------------------------------------

expr_reduce::expr_reduce(dt::pexpr&& a, size_t op) : arg(std::move(a))
{
  if (op == 0 || op >= REDUCEOP_COUNT) {
    throw ValueError() << "Invalid op code in expr_reduce: " << op;
  }
  opcode = static_cast<ReduceOp>(op);
}


SType expr_reduce::resolve(const dt::workframe& wf) {
  SType arg_stype = arg->resolve(wf);
  if (opcode == ReduceOp::FIRST) {
    return arg_stype;
  }
  auto reducer = library.lookup(opcode, arg_stype);
  if (!reducer) {
    throw TypeError() << "Unable to apply reduce function `"
        << reducer_names[static_cast<size_t>(opcode)]
        << "()` to a column of type `" << arg_stype << "`";
  }
  return reducer->output_stype;
}


dt::GroupbyMode expr_reduce::get_groupby_mode(const dt::workframe&) const {
  return dt::GroupbyMode::GtoONE;
}


dt::colptr expr_reduce::evaluate_eager(dt::workframe& wf)
{
  auto input_col = arg->evaluate_eager(wf);
  Groupby gb = wf.get_groupby();
  if (!gb) gb = Groupby::single_group(input_col->nrows);

  size_t out_nrows = gb.ngroups();
  if (!out_nrows) out_nrows = 1;  // only when input_col has 0 rows

  if (opcode == ReduceOp::FIRST) {
    return reduce_first(input_col, gb);
  }

  SType in_stype = input_col->stype();
  auto reducer = library.lookup(opcode, in_stype);
  xassert(reducer);  // checked in .resolve()

  SType out_stype = reducer->output_stype;
  auto res = colptr(Column::new_data_column(out_stype, out_nrows));

  RowIndex rowindex = input_col->rowindex();
  if (opcode == ReduceOp::MEDIAN && gb) {
    rowindex = input_col->sort_grouped(rowindex, gb);
  }

  const void* input = input_col->data();
  if (in_stype == SType::STR32) input = static_cast<const char*>(input) + 4;
  if (in_stype == SType::STR64) input = static_cast<const char*>(input) + 8;
  void* output = res->data_w();

  if (out_nrows == 1) {
    reducer->f(rowindex, 0, input_col->nrows, input, output, 0);
  }
  else {
    const int32_t* groups = wf.get_groupby().offsets_r();

    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < out_nrows; ++i) {
      size_t row0 = static_cast<size_t>(groups[i]);
      size_t row1 = static_cast<size_t>(groups[i + 1]);
      reducer->f(rowindex, row0, row1, input, output, i);
    }
  }
  return res;
}




//------------------------------------------------------------------------------
// Initialization
//------------------------------------------------------------------------------

void init_reducers()
{
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
  library.add(ReduceOp::SUM, sum_reducer<float,   float>,    SType::FLOAT32, SType::FLOAT32);
  library.add(ReduceOp::SUM, sum_reducer<double,  double>,   SType::FLOAT64, SType::FLOAT64);

  // Mean
  library.add(ReduceOp::MEAN, mean_reducer<int8_t,  double>,  SType::BOOL, SType::FLOAT64);
  library.add(ReduceOp::MEAN, mean_reducer<int8_t,  double>,  SType::INT8, SType::FLOAT64);
  library.add(ReduceOp::MEAN, mean_reducer<int16_t, double>,  SType::INT16, SType::FLOAT64);
  library.add(ReduceOp::MEAN, mean_reducer<int32_t, double>,  SType::INT32, SType::FLOAT64);
  library.add(ReduceOp::MEAN, mean_reducer<int64_t, double>,  SType::INT64, SType::FLOAT64);
  library.add(ReduceOp::MEAN, mean_reducer<float,   float>,   SType::FLOAT32, SType::FLOAT32);
  library.add(ReduceOp::MEAN, mean_reducer<double,  double>,  SType::FLOAT64, SType::FLOAT64);

  // Standard Deviation
  library.add(ReduceOp::STDEV, stdev_reducer<int8_t,  double>,  SType::BOOL, SType::FLOAT64);
  library.add(ReduceOp::STDEV, stdev_reducer<int8_t,  double>,  SType::INT8, SType::FLOAT64);
  library.add(ReduceOp::STDEV, stdev_reducer<int16_t, double>,  SType::INT16, SType::FLOAT64);
  library.add(ReduceOp::STDEV, stdev_reducer<int32_t, double>,  SType::INT32, SType::FLOAT64);
  library.add(ReduceOp::STDEV, stdev_reducer<int64_t, double>,  SType::INT64, SType::FLOAT64);
  library.add(ReduceOp::STDEV, stdev_reducer<float,   float>,   SType::FLOAT32, SType::FLOAT32);
  library.add(ReduceOp::STDEV, stdev_reducer<double,  double>,  SType::FLOAT64, SType::FLOAT64);

  // Median
  library.add(ReduceOp::MEDIAN, median_reducer<int8_t, double>,  SType::BOOL, SType::FLOAT64);
  library.add(ReduceOp::MEDIAN, median_reducer<int8_t, double>,  SType::INT8, SType::FLOAT64);
  library.add(ReduceOp::MEDIAN, median_reducer<int16_t, double>, SType::INT16, SType::FLOAT64);
  library.add(ReduceOp::MEDIAN, median_reducer<int32_t, double>, SType::INT32, SType::FLOAT64);
  library.add(ReduceOp::MEDIAN, median_reducer<int64_t, double>, SType::INT64, SType::FLOAT64);
  library.add(ReduceOp::MEDIAN, median_reducer<float, float>,    SType::FLOAT32, SType::FLOAT32);
  library.add(ReduceOp::MEDIAN, median_reducer<double, double>,  SType::FLOAT64, SType::FLOAT64);
}


};  // namespace expr
