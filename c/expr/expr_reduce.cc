//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <cmath>              // std::sqrt
#include <limits>             // std::numeric_limits<?>::max, ::infinity
#include <memory>             // std::unique_ptr
#include <unordered_map>      // std::unordered_map
#include "expr/expr_reduce.h"
#include "expr/eval_context.h"
#include "parallel/api.h"
#include "types.h"
namespace dt {
namespace expr {


template<typename T>
constexpr T infinity() {
  return std::numeric_limits<T>::has_infinity
         ? std::numeric_limits<T>::infinity()
         : std::numeric_limits<T>::max();
}

static const char* reducer_names[REDUCER_COUNT] = {
  "mean", "min", "max", "stdev", "first", "sum", "count", "count", "median"
};



//------------------------------------------------------------------------------
// Reducer library
//------------------------------------------------------------------------------
using reducer_fn = void (*)(const Column& ri, size_t row0, size_t row1,
                            void* output, size_t grp);

struct Reducer {
  reducer_fn f;
  SType output_stype;
  size_t : 56;
};


class ReducerLibrary {
  private:
    std::unordered_map<size_t, Reducer> reducers;

  public:
    void add(Op op, reducer_fn f, SType inp_stype, SType out_stype) {
      size_t id = key(op, inp_stype);
      xassert(reducers.count(id) == 0);
      reducers[id] = Reducer {f, out_stype};
    }

    const Reducer* lookup(Op op, SType stype) const {
      size_t id = key(op, stype);
      if (reducers.count(id) == 0) return nullptr;
      return &(reducers.at(id));
    }

  private:
    static constexpr size_t key(Op op, SType stype) {
      return static_cast<size_t>(op) +
             REDUCER_COUNT * static_cast<size_t>(stype);
    }
};


static ReducerLibrary library;




//------------------------------------------------------------------------------
// "First" reducer
//------------------------------------------------------------------------------

static Column reduce_first(const Column& col, const Groupby& groupby)
{
  if (col.nrows() == 0) {
    return Column::new_data_column(0, col.stype());
  }
  size_t ngrps = groupby.ngroups();
  // groupby.offsets array has length `ngrps + 1` and contains offsets of the
  // beginning of each group. We will take this array and reinterpret it as a
  // RowIndex (taking only the first `ngrps` elements). Applying this rowindex
  // to the column will produce the vector of first elements in that column.
  arr32_t indices(ngrps);
  // TODO: Groupby should use Buffer, then data copying can be shallow
  std::memcpy(indices.data(), groupby.offsets_r(), ngrps * 4);
  Column res = col;  // copy
  res.apply_rowindex(RowIndex(std::move(indices), true));
  if (ngrps == 1) res.materialize();
  return res;
}



//------------------------------------------------------------------------------
// Sum calculation
//------------------------------------------------------------------------------

template<typename T, typename U>
static void sum_reducer(const Column& input_col, size_t row0, size_t row1,
                        void* out, size_t grp_index)
{
  U sum = 0;
  for (size_t j = row0; j < row1; ++j) {
    T value;
    bool isvalid = input_col.get_element(j, &value);
    if (isvalid) {
      sum += static_cast<U>(value);
    }
  }
  U* outputs = static_cast<U*>(out);
  outputs[grp_index] = sum;
}



//------------------------------------------------------------------------------
// Count calculation
//------------------------------------------------------------------------------

template<typename T>
static void count_reducer(const Column& input_col, size_t row0, size_t row1,
                          void* out, size_t grp_index)
{
  T tmp;
  int64_t count = 0;
  for (size_t j = row0; j < row1; ++j) {
    bool isvalid = input_col.get_element(j, &tmp);
    count += isvalid;
  }
  int64_t* outputs = static_cast<int64_t*>(out);
  outputs[grp_index] = count;
}



//------------------------------------------------------------------------------
// Mean calculation
//------------------------------------------------------------------------------

template<typename T, typename U>
static void mean_reducer(const Column& input_col, size_t row0, size_t row1,
                         void* out, size_t grp_index)
{
  T value;
  U sum = 0;
  int64_t count = 0;
  for (size_t j = row0; j < row1; ++j) {
    bool isvalid = input_col.get_element(j, &value);
    if (isvalid) {
      sum += static_cast<U>(value);
      count++;
    }
  }
  U* outputs = static_cast<U*>(out);
  outputs[grp_index] = (count == 0)? GETNA<U>() : sum / count;
}



//------------------------------------------------------------------------------
// Standard deviation
//------------------------------------------------------------------------------

// Welford algorithm
template<typename T, typename U>
static void stdev_reducer(const Column& input_col, size_t row0, size_t row1,
                          void* out, size_t grp_index)
{
  U mean = 0;
  U m2 = 0;
  T value;
  int64_t count = 0;
  for (size_t j = row0; j < row1; ++j) {
    bool isvalid = input_col.get_element(j, &value);
    if (isvalid) {
      count++;
      U tmp1 = static_cast<U>(value) - mean;
      mean += tmp1 / count;
      U tmp2 = static_cast<U>(value) - mean;
      m2 += tmp1 * tmp2;
    }
  }
  U* outputs = static_cast<U*>(out);
  outputs[grp_index] = (count <= 1)? GETNA<U>() : std::sqrt(m2/(count - 1));
}



//------------------------------------------------------------------------------
// Minimum
//------------------------------------------------------------------------------

template<typename T>
static void min_reducer(const Column& input_col, size_t row0, size_t row1,
                        void* out, size_t grp_index)
{
  T value;
  T res = infinity<T>();
  bool valid = false;
  for (size_t j = row0; j < row1; ++j) {
    bool isvalid = input_col.get_element(j, &value);
    if (isvalid) {
      if (value < res) res = value;
      valid = true;
    }
  }
  T* outputs = static_cast<T*>(out);
  outputs[grp_index] = valid? res : GETNA<T>();
}



//------------------------------------------------------------------------------
// Maximum
//------------------------------------------------------------------------------

template<typename T>
static void max_reducer(const Column& input_col, size_t row0, size_t row1,
                        void* out, size_t grp_index)
{
  T value;
  T res = -infinity<T>();
  bool valid = false;
  for (size_t j = row0; j < row1; ++j) {
    bool isvalid = input_col.get_element(j, &value);
    if (isvalid) {
      if (value > res) res = value;
      valid = true;
    }
  }
  T* outputs = static_cast<T*>(out);
  outputs[grp_index] = valid? res : GETNA<T>();
}



//------------------------------------------------------------------------------
// Median
//------------------------------------------------------------------------------

template<typename T, typename U>
static void median_reducer(const Column& input_col, size_t row0, size_t row1,
                           void* out, size_t grp_index)
{
  // skip NA values if any
  T value;
  bool isvalid;
  for (; row0 < row1; ++row0) {
    isvalid = input_col.get_element(row0, &value);
    if (isvalid) break;
  }

  U* outputs = static_cast<U*>(out);
  if (row0 == row1) {
    outputs[grp_index] = GETNA<U>();
  } else {
    size_t j = (row1 + row0) / 2;
    isvalid = input_col.get_element(j, &value);
    xassert(isvalid);
    if ((row1 - row0) & 1) {
      outputs[grp_index] = static_cast<U>(value);
    }
    else {
      T value2;
      isvalid = input_col.get_element(j-1, &value2);
      xassert(isvalid);
      outputs[grp_index] = (static_cast<U>(value) + static_cast<U>(value2))/2;
    }
  }
}



//------------------------------------------------------------------------------
// expr_reduce1
//------------------------------------------------------------------------------

expr_reduce1::expr_reduce1(pexpr&& a, Op op)
  : arg(std::move(a)), opcode(op) {}


SType expr_reduce1::resolve(const EvalContext& ctx) {
  SType arg_stype = arg->resolve(ctx);
  if (opcode == Op::FIRST) {
    return arg_stype;
  }
  auto reducer = library.lookup(opcode, arg_stype);
  if (!reducer) {
    throw TypeError() << "Unable to apply reduce function `"
        << reducer_names[static_cast<size_t>(opcode) - REDUCER_FIRST]
        << "()` to a column of type `" << arg_stype << "`";
  }
  return reducer->output_stype;
}


GroupbyMode expr_reduce1::get_groupby_mode(const EvalContext&) const {
  return GroupbyMode::GtoONE;
}


Column expr_reduce1::evaluate(EvalContext& ctx)
{
  auto input_col = arg->evaluate(ctx);
  Groupby gb = ctx.get_groupby();
  size_t out_nrows = gb.ngroups();

  if (opcode == Op::FIRST) {
    return reduce_first(input_col, gb);
  }
  if (opcode == Op::MEDIAN) {
    input_col.sort_grouped(gb);
  }

  SType in_stype = input_col.stype();
  auto reducer = library.lookup(opcode, in_stype);
  xassert(reducer);  // checked in .resolve()

  SType out_stype = reducer->output_stype;
  auto res = Column::new_data_column(out_nrows, out_stype);
  void* output = res.get_data_editable();

  if (out_nrows == 1) {
    reducer->f(input_col, 0, input_col.nrows(), output, 0);
  }
  else {
    const int32_t* groups = ctx.get_groupby().offsets_r();

    parallel_for_dynamic(out_nrows,
      [&](size_t i) {
        size_t row0 = static_cast<size_t>(groups[i]);
        size_t row1 = static_cast<size_t>(groups[i + 1]);
        reducer->f(input_col, row0, row1, output, i);
      });
  }
  return res;
}




//------------------------------------------------------------------------------
// expr_reduce0
//------------------------------------------------------------------------------

expr_reduce0::expr_reduce0(Op op)
  : opcode(op) {}


SType expr_reduce0::resolve(const EvalContext&) {
  return SType::INT64;
}


GroupbyMode expr_reduce0::get_groupby_mode(const EvalContext&) const {
  return GroupbyMode::GtoONE;
}


Column expr_reduce0::evaluate(EvalContext& ctx) {
  Column res;
  if (opcode == Op::COUNT0) {  // COUNT
    if (ctx.has_groupby()) {
      const Groupby& grpby = ctx.get_groupby();
      size_t ng = grpby.ngroups();
      const int32_t* offsets = grpby.offsets_r();
      res = Column::new_data_column(ng, SType::INT64);
      auto d_res = static_cast<int64_t*>(res.get_data_editable());
      for (size_t i = 0; i < ng; ++i) {
        d_res[i] = static_cast<int64_t>(offsets[i + 1] - offsets[i]);
      }
    } else {
      res = Column::new_data_column(1, SType::INT64);
      auto d_res = static_cast<int64_t*>(res.get_data_editable());
      d_res[0] = static_cast<int64_t>(ctx.nrows());
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
  library.add(Op::COUNT, count_reducer<int8_t>,  SType::BOOL, SType::INT64);
  library.add(Op::COUNT, count_reducer<int8_t>,  SType::INT8, SType::INT64);
  library.add(Op::COUNT, count_reducer<int16_t>, SType::INT16, SType::INT64);
  library.add(Op::COUNT, count_reducer<int32_t>, SType::INT32, SType::INT64);
  library.add(Op::COUNT, count_reducer<int64_t>, SType::INT64, SType::INT64);
  library.add(Op::COUNT, count_reducer<float>,   SType::FLOAT32, SType::INT64);
  library.add(Op::COUNT, count_reducer<double>,  SType::FLOAT64, SType::INT64);
  library.add(Op::COUNT, count_reducer<CString>, SType::STR32, SType::INT64);
  library.add(Op::COUNT, count_reducer<CString>, SType::STR64, SType::INT64);

  // Min
  library.add(Op::MIN, min_reducer<int8_t>,  SType::BOOL, SType::BOOL);
  library.add(Op::MIN, min_reducer<int8_t>,  SType::INT8, SType::INT8);
  library.add(Op::MIN, min_reducer<int16_t>, SType::INT16, SType::INT16);
  library.add(Op::MIN, min_reducer<int32_t>, SType::INT32, SType::INT32);
  library.add(Op::MIN, min_reducer<int64_t>, SType::INT64, SType::INT64);
  library.add(Op::MIN, min_reducer<float>,   SType::FLOAT32, SType::FLOAT32);
  library.add(Op::MIN, min_reducer<double>,  SType::FLOAT64, SType::FLOAT64);

  // Max
  library.add(Op::MAX, max_reducer<int8_t>,  SType::BOOL, SType::BOOL);
  library.add(Op::MAX, max_reducer<int8_t>,  SType::INT8, SType::INT8);
  library.add(Op::MAX, max_reducer<int16_t>, SType::INT16, SType::INT16);
  library.add(Op::MAX, max_reducer<int32_t>, SType::INT32, SType::INT32);
  library.add(Op::MAX, max_reducer<int64_t>, SType::INT64, SType::INT64);
  library.add(Op::MAX, max_reducer<float>,   SType::FLOAT32, SType::FLOAT32);
  library.add(Op::MAX, max_reducer<double>,  SType::FLOAT64, SType::FLOAT64);

  // Sum
  library.add(Op::SUM, sum_reducer<int8_t,  int64_t>,  SType::BOOL, SType::INT64);
  library.add(Op::SUM, sum_reducer<int8_t,  int64_t>,  SType::INT8, SType::INT64);
  library.add(Op::SUM, sum_reducer<int16_t, int64_t>,  SType::INT16, SType::INT64);
  library.add(Op::SUM, sum_reducer<int32_t, int64_t>,  SType::INT32, SType::INT64);
  library.add(Op::SUM, sum_reducer<int64_t, int64_t>,  SType::INT64, SType::INT64);
  library.add(Op::SUM, sum_reducer<float,   float>,    SType::FLOAT32, SType::FLOAT32);
  library.add(Op::SUM, sum_reducer<double,  double>,   SType::FLOAT64, SType::FLOAT64);

  // Mean
  library.add(Op::MEAN, mean_reducer<int8_t,  double>,  SType::BOOL, SType::FLOAT64);
  library.add(Op::MEAN, mean_reducer<int8_t,  double>,  SType::INT8, SType::FLOAT64);
  library.add(Op::MEAN, mean_reducer<int16_t, double>,  SType::INT16, SType::FLOAT64);
  library.add(Op::MEAN, mean_reducer<int32_t, double>,  SType::INT32, SType::FLOAT64);
  library.add(Op::MEAN, mean_reducer<int64_t, double>,  SType::INT64, SType::FLOAT64);
  library.add(Op::MEAN, mean_reducer<float,   float>,   SType::FLOAT32, SType::FLOAT32);
  library.add(Op::MEAN, mean_reducer<double,  double>,  SType::FLOAT64, SType::FLOAT64);

  // Standard Deviation
  library.add(Op::STDEV, stdev_reducer<int8_t,  double>,  SType::BOOL, SType::FLOAT64);
  library.add(Op::STDEV, stdev_reducer<int8_t,  double>,  SType::INT8, SType::FLOAT64);
  library.add(Op::STDEV, stdev_reducer<int16_t, double>,  SType::INT16, SType::FLOAT64);
  library.add(Op::STDEV, stdev_reducer<int32_t, double>,  SType::INT32, SType::FLOAT64);
  library.add(Op::STDEV, stdev_reducer<int64_t, double>,  SType::INT64, SType::FLOAT64);
  library.add(Op::STDEV, stdev_reducer<float,   float>,   SType::FLOAT32, SType::FLOAT32);
  library.add(Op::STDEV, stdev_reducer<double,  double>,  SType::FLOAT64, SType::FLOAT64);

  // Median
  library.add(Op::MEDIAN, median_reducer<int8_t, double>,  SType::BOOL, SType::FLOAT64);
  library.add(Op::MEDIAN, median_reducer<int8_t, double>,  SType::INT8, SType::FLOAT64);
  library.add(Op::MEDIAN, median_reducer<int16_t, double>, SType::INT16, SType::FLOAT64);
  library.add(Op::MEDIAN, median_reducer<int32_t, double>, SType::INT32, SType::FLOAT64);
  library.add(Op::MEDIAN, median_reducer<int64_t, double>, SType::INT64, SType::FLOAT64);
  library.add(Op::MEDIAN, median_reducer<float, float>,    SType::FLOAT32, SType::FLOAT32);
  library.add(Op::MEDIAN, median_reducer<double, double>,  SType::FLOAT64, SType::FLOAT64);
}


}}  // namespace dt::expr
