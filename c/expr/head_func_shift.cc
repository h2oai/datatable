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
#include "column/shift.h"
#include "expr/eval_context.h"
#include "expr/expr.h"
#include "expr/head_func.h"
#include "expr/workframe.h"
#include "frame/py_frame.h"
#include "parallel/api.h"
#include "python/obj.h"
#include "datatablemodule.h"
#include "groupby.h"
#include "rowindex.h"
namespace dt {
namespace expr {




template <bool LAG>
static RowIndex compute_lag_rowindex(const Groupby& groupby, int shift) {
  xassert(shift > 0);
  arr32_t arr_indices(groupby.last_offset());
  int32_t* indices = arr_indices.data();

  auto group_offsets = groupby.offsets_r();
  dt::parallel_for_dynamic(
    groupby.size(),
    [&](size_t i) {
      int32_t j0 = group_offsets[i];
      int32_t j2 = group_offsets[i + 1];
      int32_t j = j0;
      if (LAG) {
        int32_t j1 = std::min(j2, j0 + shift);
        for (; j < j1; ++j) indices[j] = RowIndex::NA_ARR32;
        for (; j < j2; ++j) indices[j] = j - shift;
      } else {
        int32_t j1 = std::max(j0, j2 - shift);
        for (; j < j1; ++j) indices[j] = j + shift;
        for (; j < j2; ++j) indices[j] = RowIndex::NA_ARR32;
      }
    });

  return RowIndex(std::move(arr_indices), true);
}




//------------------------------------------------------------------------------
// Head_Func_Shift
//------------------------------------------------------------------------------

Head_Func_Shift::Head_Func_Shift(int shift)
  : shift_(shift) {}


ptrHead Head_Func_Shift::make(Op, const py::otuple& params) {
  xassert(params.size() == 1);
  int shift = params[0].to_int32_strict();
  return ptrHead(new Head_Func_Shift(shift));
}



Workframe Head_Func_Shift::evaluate_n(
    const vecExpr& args, EvalContext& ctx, bool) const
{
  xassert(args.size() == 1);
  Workframe inputs = args[0].evaluate_n(ctx);
  if (shift_ == 0) {
    // do nothing
  }
  else if (ctx.has_groupby()) {
    inputs.increase_grouping_mode(Grouping::GtoALL);
    const Groupby& groupby = ctx.get_groupby();
    // TODO: memoize this object within ctx
    RowIndex ri = shift_ > 0 ? compute_lag_rowindex<true>(groupby, shift_)
                             : compute_lag_rowindex<false>(groupby, -shift_);
    for (size_t i = 0; i < inputs.ncols(); ++i) {
      Column coli = inputs.retrieve_column(i);
      coli.apply_rowindex(ri);
      inputs.replace_column(i, std::move(coli));
    }
  }
  else {
    for (size_t i = 0; i < inputs.ncols(); ++i) {
      Column coli = inputs.retrieve_column(i);
      size_t nrows = coli.nrows();
      // coli.apply_rowindex(ri);
      if (shift_ > 0) {
        coli = Column(new Shift_ColumnImpl<true>(
                        std::move(coli), static_cast<size_t>(shift_), nrows));
      } else {
        coli = Column(new Shift_ColumnImpl<false>(
                        std::move(coli), static_cast<size_t>(-shift_), nrows));
      }
      inputs.replace_column(i, std::move(coli));
    }
  }
  return inputs;
}




}}  // dt::expr


//------------------------------------------------------------------------------
// Python interface
//------------------------------------------------------------------------------
namespace py {


static const char* doc_shift =
R"(shift(col, n=1)
--

Produce a column obtained from `col` shifting it  `n` rows forward.

The shift amount, `n`, can be both positive and negative. If positive,
a "lag" column is created, if negative it will be a "lead" column.

The shifted column will have the same number of rows as the original
column, with `n` observations in the beginning becoming missing, and
`n` observations at the end discarded.

This function is group-aware, i.e. in the presence of a groupby it
will perform the shift separately within each group.
)";

static PKArgs args_shift(
    1, 1, 0, false, false, {"col", "n"}, "shift", doc_shift);



static oobj make_pyexpr(dt::expr::Op opcode, otuple targs, otuple tparams) {
  size_t op = static_cast<size_t>(opcode);
  return robj(Expr_Type).call({ oint(op), targs, tparams });
}

static oobj _shift_frame(oobj arg, int n) {
  auto slice_all = oslice(oslice::NA, oslice::NA, oslice::NA);
  auto f_all = make_pyexpr(dt::expr::Op::COL,
                           otuple{ slice_all },
                           otuple{ oint(0) });
  auto shiftexpr = make_pyexpr(dt::expr::Op::SHIFTFN,
                               otuple{ f_all },
                               otuple{ oint(n) });
  auto frame = static_cast<Frame*>(arg.to_borrowed_ref());
  return frame->m__getitem__(otuple{ slice_all, shiftexpr });
}


/**
  * Python-facing function that implements an unary operator / single-
  * argument function. This function can take as an argument either a
  * python scalar, or an f-expression, or a Frame (in which case the
  * function is applied to all elements of the frame).
  */
static oobj pyfn_shift(const PKArgs& args)
{
  int32_t n = args[1].to<int32_t>(1);
  if (args[0].is_none_or_undefined()) {
    throw TypeError() << "Function `shift()` requires 1 positional argument, "
                         "but none were given";
  }
  oobj arg0 = args[0].to_oobj();
  if (arg0.is_frame()) {
    return _shift_frame(arg0, n);
  }
  if (arg0.is_dtexpr()) {
    return make_pyexpr(dt::expr::Op::SHIFTFN,
                       otuple{ arg0 }, otuple{ oint(n) });
  }
  throw TypeError() << "The first argument to `shift()` must be a column "
      "expression or a Frame, instead got " << arg0.typeobj();
}



void DatatableModule::init_methods_shift() {
  ADD_FN(&pyfn_shift, args_shift);
}




}
