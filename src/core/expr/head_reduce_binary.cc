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
#include "column/latent.h"
#include "expr/eval_context.h"
#include "expr/expr.h"
#include "expr/head_reduce.h"
#include "expr/workframe.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "column.h"
namespace dt {
namespace expr {


template <typename T>
using reducer_fn = bool(*)(const Column&, const Column&, size_t, size_t, T*);

using maker_fn = Column(*)(Column&&, Column&&, const Groupby&);

static const char* _name(Op op) {
  return (op == Op::COV)? "cov" :
         (op == Op::CORR)? "corr" :
         "??";
}


static Column make_na_result(Column&& arg1, Column&& arg2, const Groupby& gby) {
  SType st = (arg1.stype() == SType::FLOAT32 && arg2.stype() == SType::FLOAT32)
             ? SType::FLOAT32 : SType::FLOAT64;
  return Column::new_na_column(gby.size(), st);
}



//------------------------------------------------------------------------------
// BinaryReduced_ColumnImpl
//------------------------------------------------------------------------------

template <typename T>
class BinaryReduced_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column arg1;
    Column arg2;
    Groupby groupby;
    reducer_fn<T> reducer;

  public:
    BinaryReduced_ColumnImpl(SType stype, Column&& col1, Column&& col2,
                             const Groupby& grpby, reducer_fn<T> fn)
      : Virtual_ColumnImpl(grpby.size(), stype),
        arg1(std::move(col1)),
        arg2(std::move(col2)),
        groupby(grpby),
        reducer(fn)
    {
      assert_compatible_type<T>(stype);
    }

    ColumnImpl* clone() const override {
      return new BinaryReduced_ColumnImpl<T>(
                  stype_, Column(arg1), Column(arg2), groupby, reducer);
    }

    bool get_element(size_t i, T* out) const override {
      size_t i0, i1;
      groupby.get_group(i, &i0, &i1);
      return reducer(arg1, arg2, i0, i1, out);
    }
};





//------------------------------------------------------------------------------
// cov(X, Y)
//------------------------------------------------------------------------------

template <typename T>
static bool cov_reducer(const Column& col1, const Column& col2,
                        size_t i0, size_t i1, T* out)
{
  T mean1 = 0, mean2 = 0;
  T cov = 0;
  T value1, value2;
  int64_t n = 0;
  for (size_t i = i0; i < i1; ++i) {
    bool isvalid1 = col1.get_element(i, &value1);
    bool isvalid2 = col2.get_element(i, &value2);
    if (isvalid1 && isvalid2) {
      n++;
      T delta1 = value1 - mean1;
      T delta2 = value2 - mean2;
      mean1 += delta1 / n;
      mean2 += delta2 / n;
      T tmp1 = value1 - mean1;  // effectively, this is delta1*(n-1)/n
      cov += tmp1 * delta2;
    }
  }
  if (n <= 1) return false;
  *out = cov/(n - 1);
  return true;  // *out is not NA
}


template <typename T>
static Column _cov(Column&& arg1, Column&& arg2, const Groupby& gby) {
  const SType st = stype_from<T>();
  arg1.cast_inplace(st);
  arg2.cast_inplace(st);
  return Column(
          new Latent_ColumnImpl(
            new BinaryReduced_ColumnImpl<T>(
                 st, std::move(arg1), std::move(arg2), gby, cov_reducer<T>
            )));
}


static Column compute_cov(Column&& arg1, Column&& arg2, const Groupby& gby) {
  xassert(arg1.nrows() == arg2.nrows());
  return (arg1.stype() == SType::FLOAT32 && arg2.stype() == SType::FLOAT32)
          ? _cov<float>(std::move(arg1), std::move(arg2), gby)
          : _cov<double>(std::move(arg1), std::move(arg2), gby);
}




//------------------------------------------------------------------------------
// corr(X, Y)
//------------------------------------------------------------------------------

template <typename T>
static bool corr_reducer(const Column& col1, const Column& col2,
                         size_t i0, size_t i1, T* out)
{
  T mean1 = 0, mean2 = 0;
  T var1 = 0, var2 = 0;
  T cov = 0;
  T value1, value2;
  int64_t n = 0;
  for (size_t i = i0; i < i1; ++i) {
    bool isvalid1 = col1.get_element(i, &value1);
    bool isvalid2 = col2.get_element(i, &value2);
    if (isvalid1 && isvalid2) {
      n++;
      T delta1 = value1 - mean1;
      T delta2 = value2 - mean2;
      mean1 += delta1 / n;
      mean2 += delta2 / n;
      T tmp1 = value1 - mean1;  // effectively, this is delta1*(n-1)/n
      T tmp2 = value2 - mean2;
      cov += tmp1 * delta2;
      var1 += tmp1 * delta1;
      var2 += tmp2 * delta2;
    }
  }
  T var1var2 = var1 * var2;
  if (n > 1 && var1var2 > 0) {
    *out = cov / std::sqrt(var1var2);
    return true;
  } else {
    return false;
  }
}


template <typename T>
static Column _corr(Column&& arg1, Column&& arg2, const Groupby& gby) {
  const SType st = stype_from<T>();
  arg1.cast_inplace(st);
  arg2.cast_inplace(st);
  return Column(
          new Latent_ColumnImpl(
            new BinaryReduced_ColumnImpl<T>(
                 st, std::move(arg1), std::move(arg2), gby, corr_reducer<T>
            )));
}


static Column compute_corr(Column&& arg1, Column&& arg2, const Groupby& gby) {
  xassert(arg1.nrows() == arg2.nrows());
  return (arg1.stype() == SType::FLOAT32 && arg2.stype() == SType::FLOAT32)
          ? _corr<float>(std::move(arg1), std::move(arg2), gby)
          : _corr<double>(std::move(arg1), std::move(arg2), gby);
}




//------------------------------------------------------------------------------
// Head_Reduce_Binary factory function
//------------------------------------------------------------------------------

Workframe Head_Reduce_Binary::evaluate_n(
    const vecExpr& args, EvalContext& ctx, bool) const
{
  xassert(args.size() == 2);
  Workframe inputs1 = args[0].evaluate_n(ctx);
  Workframe inputs2 = args[1].evaluate_n(ctx);
  Groupby gby = ctx.get_groupby();
  if (!gby) gby = Groupby::single_group(ctx.nrows());

  maker_fn fn = nullptr;
  if (inputs1.get_grouping_mode() == Grouping::GtoALL &&
      inputs2.get_grouping_mode() == Grouping::GtoALL)
  {
    switch (op) {
      case Op::COV:  fn = compute_cov; break;
      case Op::CORR: fn = compute_corr; break;
      default: throw TypeError() << "Unknown reducer function: "
                                 << static_cast<size_t>(op);
    }
  } else {
    fn = make_na_result;
  }

  size_t n1 = inputs1.ncols();
  size_t n2 = inputs2.ncols();
  if (!(n1 == n2 || n1 == 1 || n2 == 1)) {
    throw ValueError() << "Cannot apply reducer function " << _name(op)
        << ": argument 1 has " << n1 << " columns, while argument 2 has "
        << n2 << " columns";
  }
  Column col1 = (n1 == 1)? inputs1.retrieve_column(0) : Column();
  Column col2 = (n2 == 1)? inputs2.retrieve_column(0) : Column();

  size_t n = std::max(n1, n2);
  Workframe outputs(ctx);
  for (size_t i = 0; i < n; ++i) {
    Column arg1 = (n1 == 1)? Column(col1) : inputs1.retrieve_column(i);
    Column arg2 = (n2 == 1)? Column(col2) : inputs2.retrieve_column(i);
    outputs.add_column(
        fn(std::move(arg1), std::move(arg2), gby),
        "", Grouping::GtoONE);
  }
  return outputs;
}




}}  // namespace dt::expr
