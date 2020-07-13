//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#include "_dt.h"
#include "column/cut.h"
#include "datatablemodule.h"
#include "expr/eval_context.h"
#include "expr/head_func.h"
#include "expr/head_func_cut.h"
#include "frame/py_frame.h"
#include "parallel/api.h"


namespace dt {
namespace expr {


/**
 *  Cut a workframe in-place by cutting its every column.
 */
void cut_wf(Workframe& wf, const sztvec& bins) {
  const size_t ncols = wf.ncols();
  xassert(ncols == bins.size());

  for (size_t i = 0; i < ncols; ++i) {
    Column coli = wf.retrieve_column(i);

    switch (coli.stype()) {
      case dt::SType::BOOL:    coli = Column(new Cut_ColumnImpl<int8_t, int64_t>(
                                        coli, bins[i]
                                      ));
                               break;
      case dt::SType::INT8:    coli = Column(new Cut_ColumnImpl<int8_t, int64_t>(
                                        coli, bins[i]
                                      ));
                               break;
      case dt::SType::INT16:   coli = Column(new Cut_ColumnImpl<int16_t, int64_t>(
                                        coli, bins[i]
                                      ));
                               break;
      case dt::SType::INT32:   coli = Column(new Cut_ColumnImpl<int32_t, int64_t>(
                                        coli, bins[i]
                                      ));
                               break;
      case dt::SType::INT64:   coli = Column(new Cut_ColumnImpl<int64_t, int64_t>(
                                        coli, bins[i]
                                      ));
                               break;
      case dt::SType::FLOAT32: coli = Column(new Cut_ColumnImpl<float, double>(
                                        coli, bins[i]
                                      ));
                               break;
      case dt::SType::FLOAT64: coli = Column(new Cut_ColumnImpl<double, double>(
                                        coli, bins[i]
                                      ));
                               break;
      default:  throw TypeError() << "cut() can only be applied to numeric "
                  << "columns, instead column `" << i << "` has an stype: `"
                  << coli.stype() << "`";
    }
    wf.replace_column(i, std::move(coli));
  }
}



//------------------------------------------------------------------------------
// Head_Func_Cut
//------------------------------------------------------------------------------

Head_Func_Cut::Head_Func_Cut(py::oobj py_bins)
  : py_bins_(py_bins) {}


ptrHead Head_Func_Cut::make(Op, const py::otuple& params) {
  xassert(params.size() == 1);
  py::oobj py_bins = params[0];
  return ptrHead(new Head_Func_Cut(py_bins));
}



Workframe Head_Func_Cut::evaluate_n(
    const vecExpr& args, EvalContext& ctx, bool) const
{
  if (ctx.has_groupby()) {
    throw NotImplError() << "cut() cannot be used in a groupby context";
  }

  Workframe wf = args[0].evaluate_n(ctx);
  const size_t ncols = wf.ncols();

  sztvec bins(ncols);
  size_t bins_default = 10;
  bool defined_bins = !py_bins_.is_none();
  bool bins_list_or_tuple = py_bins_.is_list_or_tuple();

  if (bins_list_or_tuple) {
    py::oiter py_bins = py_bins_.to_oiter();
    if (py_bins.size() != ncols) {
      throw ValueError() << "When `bins` is a list or a tuple, its length must be "
        << "the same as the number of columns in the frame/expression, i.e. `"
        << ncols << "`, instead got: `" << py_bins.size() << "`";

    }

    size_t i = 0;
    for (auto py_bin : py_bins) {
      size_t bin = py_bin.to_size_t();
      bins[i++] = bin;
    }
    xassert(i == ncols);

  } else {
    if (defined_bins) bins_default = py_bins_.to_size_t();
    for (size_t i = 0; i < ncols; ++i) {
      bins[i] = bins_default;
    }
  }

  // Cut workframe in-place
  cut_wf(wf, bins);
  return wf;
}

}}  // dt::expr


namespace py {

static oobj make_pyexpr(dt::expr::Op opcode, otuple targs, otuple tparams) {
  size_t op = static_cast<size_t>(opcode);
  return robj(Expr_Type).call({ oint(op), targs, tparams });
}


static oobj cut_frame(oobj arg1, oobj arg2) {
  auto slice_all = oslice(oslice::NA, oslice::NA, oslice::NA);
  auto f_all = make_pyexpr(dt::expr::Op::COL,
                           otuple{ slice_all },
                           otuple{ oint(0) });
  auto cutexpr = make_pyexpr(dt::expr::Op::CUT,
                               otuple{ f_all },
                               otuple{ arg2 });
  auto frame = static_cast<Frame*>(arg1.to_borrowed_ref());
  return frame->m__getitem__(otuple{ slice_all, cutexpr });
}



static PKArgs args_cut(
  1, 1, 0, false, false,
  {
    "input", "bins"
  },
  "cut",

R"(cut(input, bins=10)
--

Bin all the columns in a Frame/f-expression.

Parameters
----------
input: Frame | f-expression
    Frame or f-expression that consists of numeric columns only.
bins: int | list or a tuple of int
    Number of bins to be used for the whole Frame/f-expression,
    or a list/tuple with the numbers of bins for the corresponding columns.
    In the latter case, the list/tuple length must be equal
    to the number of columns in the Frame/f-expression.

Returns
-------
Frame/f-expression, where each column is filled with the respective bin ids.
)"
);


/**
  * Python-facing function that can take as an argument either a Frame or
  * an f-expression.
  */
static oobj pyfn_cut(const PKArgs& args)
{
  if (args[0].is_none_or_undefined()) {
    throw TypeError() << "Function `cut()` requires one positional argument, "
                         "but none were given";
  }
  oobj arg0 = args[0].to_oobj();
  oobj arg1 = args[1].is_none_or_undefined()? py::None() : args[1].to_oobj();

  if (arg0.is_frame()) {
    return cut_frame(arg0, arg1);
  }
  if (arg0.is_dtexpr()) {
    return make_pyexpr(dt::expr::Op::CUT,
                       otuple{ arg0 }, otuple{ arg1 });
  }
  throw TypeError() << "The first argument to `cut()` must be a column "
      "expression or a Frame, instead got " << arg0.typeobj();
}



void DatatableModule::init_methods_cut() {
  ADD_FN(&py::pyfn_cut, py::args_cut);
}

}  // namespace py


