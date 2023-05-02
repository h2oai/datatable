//------------------------------------------------------------------------------
// Copyright 2019-2023 H2O.ai
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
#include "column/const.h"
#include "column/latent.h"
#include "column/virtual.h"
#include "expr/eval_context.h"
#include "expr/expr.h"
#include "expr/head_reduce.h"
#include "expr/workframe.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "stype.h"
#include <type_traits>
#include <set>
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
// Reduced_ ColumnImpl
//------------------------------------------------------------------------------

// T - type of elements in the `arg` column
// U - type of output elements from this column
//
template <typename T, typename U>
class Reduced_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column arg;
    Groupby groupby;
    reducer_fn<U> reducer;

    // Each element is "expensive" to compute if the average group
    // size is larger than this threshold.
    static constexpr size_t GROUP_SIZE_TINY = 4;

  public:
    Reduced_ColumnImpl(SType stype, Column&& col, const Groupby& grpby,
                       reducer_fn<U> fn)
      : Virtual_ColumnImpl(grpby.size(), stype),
        arg(std::move(col)),
        groupby(grpby),
        reducer(fn)
    {
      xassert(compatible_type<T>(arg.stype()));
      xassert(compatible_type<U>(stype));
    }

    ColumnImpl* clone() const override {
      return new Reduced_ColumnImpl<T, U>(stype(), Column(arg), groupby,
                                          reducer);
    }

    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return arg;
    }


    bool get_element(size_t i, U* out) const override {
      size_t i0, i1;
      groupby.get_group(i, &i0, &i1);
      return reducer(arg, i0, i1, out);
    }

    bool computationally_expensive() const override {
      return nrows_ >= GROUP_SIZE_TINY * groupby.size();
    }
};




//------------------------------------------------------------------------------
// first(A), last(A)
//------------------------------------------------------------------------------

template <bool FIRST>
class FirstLast_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column arg;
    Groupby groupby;

  public:
    FirstLast_ColumnImpl(Column&& col, const Groupby& grpby)
      : Virtual_ColumnImpl(grpby.size(), col.stype()),
        arg(std::move(col)),
        groupby(grpby) {}

    ColumnImpl* clone() const override {
      return new FirstLast_ColumnImpl<FIRST>(Column(arg), groupby);
    }

    bool get_element(size_t i, int8_t*   out) const override { return _get(i, out); }
    bool get_element(size_t i, int16_t*  out) const override { return _get(i, out); }
    bool get_element(size_t i, int32_t*  out) const override { return _get(i, out); }
    bool get_element(size_t i, int64_t*  out) const override { return _get(i, out); }
    bool get_element(size_t i, float*    out) const override { return _get(i, out); }
    bool get_element(size_t i, double*   out) const override { return _get(i, out); }
    bool get_element(size_t i, CString*  out) const override { return _get(i, out); }
    bool get_element(size_t i, py::oobj* out) const override { return _get(i, out); }


    bool computationally_expensive() const override {
      return true;
    }

    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return arg;
    }


  private:
    template <typename T>
    bool _get(size_t i, T* out) const {
      size_t i0, i1;
      groupby.get_group(i, &i0, &i1);
      xassert(i0 < i1);
      return FIRST? arg.get_element(i0, out)
                  : arg.get_element(i1 - 1, out);
    }
};


template <bool FIRST>
static Column compute_firstlast(Column&& arg, const Groupby& gby) {
  if (arg.nrows() == 0) {
    return Column::new_na_column(1, arg.stype());
  }
  else {
    return Column(new FirstLast_ColumnImpl<FIRST>(std::move(arg), gby));
  }
}


static Column compute_gfirstlast(Column&& arg, const Groupby&) {
  return (arg.nrows() == 0)? Column::new_na_column(1, arg.stype())
                           : std::move(arg);
}







//------------------------------------------------------------------------------
// sd(A)
//------------------------------------------------------------------------------

template <typename T, typename U>
bool sd_reducer(const Column& col, size_t i0, size_t i1, U* out) {
  double mean = 0;
  double m2 = 0;
  T value;
  int64_t count = 0;
  for (size_t i = i0; i < i1; ++i) {
    bool isvalid = col.get_element(i, &value);
    if (isvalid) {
      count++;
      double tmp1 = static_cast<double>(value) - mean;
      mean += tmp1 / static_cast<double>(count);
      double tmp2 = static_cast<double>(value) - mean;
      m2 += tmp1 * tmp2;
    }
  }
  if (count <= 1 || std::isnan(m2)) return false;
  // In theory, m2 should always be positive, but perhaps it could
  // occasionally become negative due to round-off errors?
  *out = static_cast<U>(m2 >= 0? std::sqrt(m2/static_cast<double>(count - 1))
                               : 0.0);
  return true;  // *out is not NA
}



template <typename T>
static Column _sd(Column&& arg, const Groupby& gby) {
  using U = typename std::conditional<std::is_same<T, float>::value,
                                      float, double>::type;
  return Column(
          new Latent_ColumnImpl(
            new Reduced_ColumnImpl<T, U>(
                 stype_from<U>, std::move(arg), gby, sd_reducer<T, U>
            )));
}

static Column compute_sd(Column&& arg, const Groupby& gby) {
  switch (arg.stype()) {
    case SType::VOID:    return Column(new ConstNa_ColumnImpl(
                                  gby.size(), SType::FLOAT64
                                ));
    case SType::BOOL:
    case SType::INT8:    return _sd<int8_t> (std::move(arg), gby);
    case SType::INT16:   return _sd<int16_t>(std::move(arg), gby);
    case SType::INT32:   return _sd<int32_t>(std::move(arg), gby);
    case SType::INT64:   return _sd<int64_t>(std::move(arg), gby);
    case SType::FLOAT32: return _sd<float>  (std::move(arg), gby);
    case SType::FLOAT64: return _sd<double> (std::move(arg), gby);
    default: throw _error("sd", arg.stype());
  }
}




//------------------------------------------------------------------------------
// sd(A:grouped)
//------------------------------------------------------------------------------

class SdGrouped_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column arg;
    Groupby groupby;

  public:
    SdGrouped_ColumnImpl(SType stype, Column&& col, const Groupby& grpby)
      : Virtual_ColumnImpl(grpby.size(), stype),
        arg(std::move(col)),
        groupby(grpby) {}

    ColumnImpl* clone() const override {
      return new SdGrouped_ColumnImpl(stype(), Column(arg), groupby);
    }

    bool get_element(size_t i, float* out) const override {
      *out = 0.0f;
      size_t i0, i1;
      groupby.get_group(i, &i0, &i1);
      return (i1 - i0 > 1);
    }

    bool get_element(size_t i, double* out) const override {
      *out = 0.0;
      size_t i0, i1;
      groupby.get_group(i, &i0, &i1);
      return (i1 - i0 > 1);
    }


    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return arg;
    }
};


static Column compute_gsd(Column&& arg, const Groupby& gby) {
  SType arg_stype = arg.stype();
  if (arg_stype == SType::STR32 || arg_stype == SType::STR64) {
    throw _error("sd", arg_stype);
  }
  SType res_stype = (arg_stype == SType::FLOAT32)? SType::FLOAT32
                                                 : SType::FLOAT64;
  if (arg.nrows() == 0 || arg_stype == SType::VOID) {
    return Column::new_na_column(1, res_stype);
  }
  return Column(new SdGrouped_ColumnImpl(res_stype, std::move(arg), gby));
}




//------------------------------------------------------------------------------
// nunique(A:grouped)
//------------------------------------------------------------------------------

// T is the type of the input column
template <typename T>
class NuniqueGrouped_ColumnImpl : public Virtual_ColumnImpl
{
  private:
    Column arg;

  public:
    NuniqueGrouped_ColumnImpl(Column&& col)
      : Virtual_ColumnImpl(col.nrows(), SType::INT64),
        arg(std::move(col)) {}

    ColumnImpl* clone() const override {
      return new NuniqueGrouped_ColumnImpl<T>(Column(arg));
    }

    bool get_element(size_t i, int64_t* out) const override {
      T value;
      *out = arg.get_element(i, &value);
      return true;
    }

    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return arg;
    }

};

template <typename T>
static Column _gnunique(Column&& arg) {
  return Column(new NuniqueGrouped_ColumnImpl<T>(std::move(arg)));
}

static Column compute_gnunique(Column&& arg, const Groupby&) {
  switch (arg.stype()) {
    case SType::VOID:    return Column(new ConstInt_ColumnImpl(1, 0, SType::INT64));
    case SType::BOOL:
    case SType::INT8:    return _gnunique<int8_t>(std::move(arg));
    case SType::INT16:   return _gnunique<int16_t>(std::move(arg));
    case SType::DATE32:
    case SType::INT32:   return _gnunique<int32_t>(std::move(arg));
    case SType::TIME64:
    case SType::INT64:   return _gnunique<int64_t>(std::move(arg));
    case SType::FLOAT32: return _gnunique<float>(std::move(arg));
    case SType::FLOAT64: return _gnunique<double>(std::move(arg));
    case SType::STR32:
    case SType::STR64:   return _gnunique<CString>(std::move(arg));
    default: throw _error("nunique", arg.stype());
  }
}



//------------------------------------------------------------------------------
// nunique
//------------------------------------------------------------------------------

template <typename T>
bool op_nunique(const Column& col, size_t i0, size_t i1, int64_t* out) {
  std::set<T> ss;
  for (size_t i = i0; i < i1; ++i) {
    T value;
    bool isvalid = col.get_element(i, &value);
    if (isvalid) ss.insert(value);
  }
  *out = static_cast<int64_t>(ss.size());
  return true;  // *out is not NA
}



template <typename T>
static Column _nunique(Column&& arg, const Groupby& gby) {
  return Column(
          new Latent_ColumnImpl(
            new Reduced_ColumnImpl<T, int64_t>(
                 SType::INT64, std::move(arg), gby, op_nunique<T>
            )));
}


static Column compute_nunique(Column&& arg, const Groupby& gby) {
  switch (arg.stype()) {
    case SType::VOID:
    case SType::BOOL:
    case SType::INT8:    return _nunique<int8_t>(std::move(arg), gby);
    case SType::INT16:   return _nunique<int16_t>(std::move(arg), gby);
    case SType::DATE32:
    case SType::INT32:   return _nunique<int32_t>(std::move(arg), gby);
    case SType::TIME64:
    case SType::INT64:   return _nunique<int64_t>(std::move(arg), gby);
    case SType::FLOAT32: return _nunique<float>(std::move(arg), gby);
    case SType::FLOAT64: return _nunique<double>(std::move(arg), gby);
    case SType::STR32:
    case SType::STR64:   return _nunique<CString>(std::move(arg), gby);
    default: throw _error("nunique", arg.stype());
  }
}



//------------------------------------------------------------------------------
// Median
//------------------------------------------------------------------------------

template <typename T, typename U>
class Median_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column arg;
    Groupby groupby;

  public:
    Median_ColumnImpl(Column&& col, const Groupby& grpby)
      : Virtual_ColumnImpl(grpby.size(), stype_from<U>),
        arg(std::move(col)),
        groupby(grpby) {}

    ColumnImpl* clone() const override {
      auto res = new Median_ColumnImpl<T, U>(Column(arg), groupby);
      return res;
    }

    void pre_materialize_hook() override {
      arg.sort_grouped(groupby);
    }

    bool get_element(size_t i, U* out) const override {
      size_t i0, i1;
      T value1, value2;
      groupby.get_group(i, &i0, &i1);
      xassert(i0 < i1);

      // skip NA values if any
      while (true) {
        bool isvalid = arg.get_element(i0, &value1);
        if (isvalid) break;
        ++i0;
        if (i0 == i1) return false;  // all elements are NA
      }

      size_t j = (i0 + i1) / 2;
      arg.get_element(j, &value1);
      if ((i1 - i0) & 1) { // Odd count of elements
        *out = static_cast<U>(value1);
      } else {
        arg.get_element(j-1, &value2);
        *out = (static_cast<U>(value1) + static_cast<U>(value2))/2;
      }
      return true;
    }


    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return arg;
    }
};


template <typename T>
static Column _median(Column&& arg, const Groupby& gby) {
  using U = typename std::conditional<std::is_same<T, float>::value,
                                      float, double>::type;
  return Column(
          new Latent_ColumnImpl(
            new Median_ColumnImpl<T, U>(std::move(arg), gby)
          ));
}

static Column compute_median(Column&& arg, const Groupby& gby) {
  if (arg.nrows() == 0) {
    return Column::new_na_column(1, arg.stype());
  }
  switch (arg.stype()) {
    case SType::VOID:    return Column(new ConstNa_ColumnImpl(
                                  gby.size(), SType::FLOAT64
                                ));
    case SType::BOOL:
    case SType::INT8:    return _median<int8_t> (std::move(arg), gby);
    case SType::INT16:   return _median<int16_t>(std::move(arg), gby);
    case SType::INT32:   return _median<int32_t>(std::move(arg), gby);
    case SType::INT64:   return _median<int64_t>(std::move(arg), gby);
    case SType::FLOAT32: return _median<float>  (std::move(arg), gby);
    case SType::FLOAT64: return _median<double> (std::move(arg), gby);
    default: throw _error("median", arg.stype());
  }
}


static Column compute_gmedian(Column&& arg, const Groupby&) {
  SType arg_stype = arg.stype();
  if (arg_stype == SType::STR32 || arg_stype == SType::STR64) {
    throw _error("median", arg_stype);
  }
  SType res_stype = (arg_stype == SType::FLOAT32)? SType::FLOAT32
                                                 : SType::FLOAT64;
  if (arg.nrows() == 0) {
    return Column::new_na_column(1, res_stype);
  }
  arg.cast_inplace(res_stype);
  return std::move(arg);
}



//------------------------------------------------------------------------------
// Head_Reduce_Unary
//------------------------------------------------------------------------------

Workframe Head_Reduce_Unary::evaluate_n(
    const vecExpr& args, EvalContext& ctx) const
{
  xassert(args.size() == 1);
  Workframe inputs = args[0]->evaluate_n(ctx);
  Groupby gby = ctx.get_groupby();
  if (!gby) gby = Groupby::single_group(ctx.nrows());

  maker_fn fn = nullptr;
  if (inputs.get_grouping_mode() == Grouping::GtoALL) {
    switch (op) {
      case Op::STDEV:  fn = compute_sd; break;
      case Op::FIRST:  fn = compute_firstlast<true>; break;
      case Op::LAST:   fn = compute_firstlast<false>; break;
      case Op::MEDIAN: fn = compute_median; break;
      case Op::NUNIQUE:fn = compute_nunique; break;
      default: throw TypeError() << "Unknown reducer function: "
                                 << static_cast<size_t>(op);
    }
  } else {
    switch (op) {
      case Op::STDEV:  fn = compute_gsd; break;
      case Op::FIRST:
      case Op::LAST:   fn = compute_gfirstlast; break;
      case Op::MEDIAN: fn = compute_gmedian; break;
      case Op::NUNIQUE:fn = compute_gnunique; break;
      default: throw TypeError() << "Unknown reducer function: "
                                 << static_cast<size_t>(op);
    }
  }

  Workframe outputs(ctx);
  for (size_t i = 0; i < inputs.ncols(); ++i) {
    outputs.add_column(
        fn(inputs.retrieve_column(i), gby),
        inputs.retrieve_name(i),
        Grouping::GtoONE);
  }
  return outputs;
}




}}  // namespace dt::expr
