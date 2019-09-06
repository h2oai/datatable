//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#include <unordered_map>
#include "expr/expr.h"
#include "expr/head_reduce.h"
#include "expr/outputs.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "column_impl.h"
namespace dt {
namespace expr {


static Error _error(const char* name, SType stype) {
  return TypeError() << "Unable to apply reduce function `"
          << name << "()` to a column of type `" << stype << "`";
}



template <typename U>
using reducer_fn = bool(*)(const Column&, size_t, size_t, U*);

using maker_fn = Column(*)(Column&&, const Groupby&);



//------------------------------------------------------------------------------
// Reduced_ColumnImpl
//------------------------------------------------------------------------------

// T - type of elements in the `arg` column
// U - type of output elements from this column
//
template <typename T, typename U>
class Reduced_ColumnImpl : public ColumnImpl {
  private:
    Column arg;
    Groupby groupby;
    reducer_fn<U> reducer;

  public:
    Reduced_ColumnImpl(SType stype, Column&& col, const Groupby& grpby,
                       reducer_fn<U> fn)
      : ColumnImpl(grpby.size(), stype),
        arg(std::move(col)),
        groupby(grpby),
        reducer(fn) {}

    bool get_element(size_t i, U* out) const override {
      size_t i0, i1;
      groupby.get_group(i, &i0, &i1);
      return reducer(arg, i0, i1, out);
    }
};




//------------------------------------------------------------------------------
// first(A)
//------------------------------------------------------------------------------

static Column compute_first(Column&& col, const Groupby& gby)
{
  if (col.nrows() == 0) {
    return Column::new_data_column(col.stype(), 0);
  }
  size_t ngrps = gby.ngroups();
  // gby.offsets array has length `ngrps + 1` and contains offsets of the
  // beginning of each group. We will take this array and reinterpret it as a
  // RowIndex (taking only the first `ngrps` elements). Applying this rowindex
  // to the column will produce the vector of first elements in that column.
  arr32_t indices(ngrps, gby.offsets_r());
  RowIndex ri = RowIndex(std::move(indices), true)
                * col->rowindex();
  Column res = col;  // copy
  res->replace_rowindex(ri);
  if (ngrps == 1) res.materialize();
  return res;
}




//------------------------------------------------------------------------------
// sum(A)
//------------------------------------------------------------------------------

template <typename T, typename U>
bool sum_reducer(const Column& col, size_t i0, size_t i1, U* out) {
  U sum = 0;
  for (size_t i = i0; i < i1; ++i) {
    T value;
    bool isna = col.get_element(i, &value);
    if (!isna) {
      sum += static_cast<U>(value);
    }
  }
  *out = sum;
  return false;  // *out is not NA
}



template <typename T, typename U>
static Column _sum(Column&& arg, const Groupby& gby) {
  return Column(
            new Reduced_ColumnImpl<T, U>(
                 stype_from<U>(), std::move(arg), gby, sum_reducer<T, U>
            ));
}

static Column compute_sum(Column&& arg, const Groupby& gby) {
  switch (arg.stype()) {
    case SType::BOOL:
    case SType::INT8:    return _sum<int8_t, int64_t>(std::move(arg), gby);
    case SType::INT16:   return _sum<int16_t, int64_t>(std::move(arg), gby);
    case SType::INT32:   return _sum<int32_t, int64_t>(std::move(arg), gby);
    case SType::INT64:   return _sum<int64_t, int64_t>(std::move(arg), gby);
    case SType::FLOAT32: return _sum<float, float>(std::move(arg), gby);
    case SType::FLOAT64: return _sum<double, double>(std::move(arg), gby);
    default: throw _error("sum", arg.stype());
  }
}




//------------------------------------------------------------------------------
// count(A)
//------------------------------------------------------------------------------

template <typename T>
bool count_reducer(const Column& col, size_t i0, size_t i1, int64_t* out) {
  int64_t count = 0;
  for (size_t i = i0; i < i1; ++i) {
    T value;
    bool isna = col.get_element(i, &value);
    count += !isna;
  }
  *out = count;
  return false;  // *out is not NA
}



template <typename T>
static Column _count(Column&& arg, const Groupby& gby) {
  return Column(
            new Reduced_ColumnImpl<T, int64_t>(
                 SType::INT64, std::move(arg), gby, count_reducer<T>
            ));
}

static Column compute_count(Column&& arg, const Groupby& gby) {
  switch (arg.stype()) {
    case SType::BOOL:
    case SType::INT8:    return _count<int8_t>(std::move(arg), gby);
    case SType::INT16:   return _count<int16_t>(std::move(arg), gby);
    case SType::INT32:   return _count<int32_t>(std::move(arg), gby);
    case SType::INT64:   return _count<int64_t>(std::move(arg), gby);
    case SType::FLOAT32: return _count<float>(std::move(arg), gby);
    case SType::FLOAT64: return _count<double>(std::move(arg), gby);
    case SType::STR32:
    case SType::STR64:   return _count<CString>(std::move(arg), gby);
    default: throw _error("count", arg.stype());
  }
}




//------------------------------------------------------------------------------
// min(A), max(A)
//------------------------------------------------------------------------------

template <typename T, bool MIN>
bool minmax_reducer(const Column& col, size_t i0, size_t i1, T* out) {
  T minmax = 0;
  bool minmax_isna = true;
  for (size_t i = i0; i < i1; ++i) {
    T value;
    bool isna = col.get_element(i, &value);
    if (isna) continue;
    if ((MIN? (value < minmax) : (value > minmax)) || minmax_isna) {
      minmax = value;
      minmax_isna = false;
    }
  }
  *out = minmax;
  return minmax_isna;
}



template <typename T, bool MM>
static Column _minmax(Column&& arg, const Groupby& gby) {
  return Column(
            new Reduced_ColumnImpl<T, T>(
                 stype_from<T>(), std::move(arg), gby, minmax_reducer<T, MM>
            ));
}

static Column compute_min(Column&& arg, const Groupby& gby) {
  switch (arg.stype()) {
    case SType::BOOL:
    case SType::INT8:    return _minmax<int8_t, true>(std::move(arg), gby);
    case SType::INT16:   return _minmax<int16_t, true>(std::move(arg), gby);
    case SType::INT32:   return _minmax<int32_t, true>(std::move(arg), gby);
    case SType::INT64:   return _minmax<int64_t, true>(std::move(arg), gby);
    case SType::FLOAT32: return _minmax<float, true>(std::move(arg), gby);
    case SType::FLOAT64: return _minmax<double, true>(std::move(arg), gby);
    default: throw _error("min", arg.stype());
  }
}

static Column compute_max(Column&& arg, const Groupby& gby) {
  switch (arg.stype()) {
    case SType::BOOL:
    case SType::INT8:    return _minmax<int8_t, false>(std::move(arg), gby);
    case SType::INT16:   return _minmax<int16_t, false>(std::move(arg), gby);
    case SType::INT32:   return _minmax<int32_t, false>(std::move(arg), gby);
    case SType::INT64:   return _minmax<int64_t, false>(std::move(arg), gby);
    case SType::FLOAT32: return _minmax<float, false>(std::move(arg), gby);
    case SType::FLOAT64: return _minmax<double, false>(std::move(arg), gby);
    default: throw _error("max", arg.stype());
  }
}




//------------------------------------------------------------------------------
// Head_Reduce_Unary
//------------------------------------------------------------------------------

Outputs Head_Reduce_Unary::evaluate(const vecExpr& args, workframe& wf) const {
  xassert(args.size() == 1);
  Outputs inputs = args[0].evaluate(wf);
  Groupby gby = wf.get_groupby();
  if (!gby) gby = Groupby::single_group(wf.nrows());

  maker_fn fn = nullptr;
  switch (op) {
    case Op::FIRST: fn = compute_first; break;
    case Op::COUNT: fn = compute_count; break;
    case Op::SUM:   fn = compute_sum; break;
    case Op::MIN:   fn = compute_min; break;
    case Op::MAX:   fn = compute_max; break;
    default: throw TypeError() << "Unknown reducer function: "
                               << static_cast<size_t>(op);
  }

  Outputs outputs(wf);
  for (size_t i = 0; i < inputs.size(); ++i) {
    Column& col = inputs.get_column(i);
    outputs.add(fn(std::move(col), gby),
                std::move(inputs.get_name(i)),
                Grouping::GtoONE);
  }
  return outputs;
}




}}  // namespace dt::expr
