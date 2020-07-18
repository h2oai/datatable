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
#include "frame/py_frame.h"
#include "parallel/api.h"


namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// Head_Func_Cut
//------------------------------------------------------------------------------

Head_Func_Cut::Head_Func_Cut(py::oobj py_nbins, py::oobj right_closed)
  : py_nbins_(py_nbins), right_closed_(right_closed.to_bool()) {}


ptrHead Head_Func_Cut::make(Op, const py::otuple& params) {
  xassert(params.size() == 2);
  return ptrHead(new Head_Func_Cut(params[0], params[1]));
}



Workframe Head_Func_Cut::evaluate_n(
    const vecExpr& args, EvalContext& ctx, bool) const
{

  if (ctx.has_groupby()) {
    throw NotImplError() << "cut() cannot be used in a groupby context";
  }

  size_t nbins_default = 10;
  Workframe wf = args[0].evaluate_n(ctx);
  const size_t ncols = wf.ncols();

  sztvec nbins(ncols);
  bool defined_nbins = !py_nbins_.is_none();
  bool nbins_list_or_tuple = py_nbins_.is_list_or_tuple();

  if (nbins_list_or_tuple) {
    py::oiter py_nbins = py_nbins_.to_oiter();
    if (py_nbins.size() != ncols) {
      throw ValueError() << "When `nbins` is a list or a tuple, its length must be "
        << "the same as the number of columns in the frame/expression, i.e. `"
        << ncols << "`, instead got: `" << py_nbins.size() << "`";

    }

    size_t i = 0;
    for (auto py_nbin : py_nbins) {
      size_t nbin = py_nbin.to_size_t();
      nbins[i++] = nbin;
    }
    xassert(i == ncols);

  } else {
    if (defined_nbins) nbins_default = py_nbins_.to_size_t();
    for (size_t i = 0; i < ncols; ++i) {
      nbins[i] = nbins_default;
    }
  }

  // Cut workframe in-place
  for (size_t i = 0; i < ncols; ++i) {
    Column coli = wf.retrieve_column(i);

    switch (coli.stype()) {
      case dt::SType::BOOL:    coli = Column(Cut_ColumnImpl::make<int64_t>(std::move(coli), nbins[i], right_closed_));
                               break;
      case dt::SType::INT8:    coli = Column(Cut_ColumnImpl::make<int64_t>(std::move(coli), nbins[i], right_closed_));
                               break;
      case dt::SType::INT16:   coli = Column(Cut_ColumnImpl::make<int64_t>(std::move(coli), nbins[i], right_closed_));
                               break;
      case dt::SType::INT32:   coli = Column(Cut_ColumnImpl::make<int64_t>(std::move(coli), nbins[i], right_closed_));
                               break;
      case dt::SType::INT64:   coli = Column(Cut_ColumnImpl::make<int64_t>(std::move(coli), nbins[i], right_closed_));
                               break;
      case dt::SType::FLOAT32: coli = Column(Cut_ColumnImpl::make<double>(std::move(coli), nbins[i], right_closed_));
                               break;
      case dt::SType::FLOAT64: coli = Column(Cut_ColumnImpl::make<double>(std::move(coli), nbins[i], right_closed_));
                               break;
      default:  throw TypeError() << "cut() can only be applied to numeric "
                  << "columns, instead column `" << i << "` has an stype: `"
                  << coli.stype() << "`";
    }
    wf.replace_column(i, std::move(coli));
  }

  return wf;
}

}}  // dt::expr


namespace py {

static oobj make_pyexpr(dt::expr::Op opcode, otuple targs, otuple tparams) {
  size_t op = static_cast<size_t>(opcode);
  return robj(Expr_Type).call({ oint(op), targs, tparams });
}


static oobj cut_frame(oobj arg0, oobj arg1, oobj arg2) {
  auto slice_all = oslice(oslice::NA, oslice::NA, oslice::NA);
  auto f_all = make_pyexpr(dt::expr::Op::COL,
                           otuple{ slice_all },
                           otuple{ oint(0) });
  auto cutexpr = make_pyexpr(dt::expr::Op::CUT,
                               otuple{ f_all },
                               otuple{ arg1, arg2 });
  auto frame = static_cast<Frame*>(arg0.to_borrowed_ref());
  return frame->m__getitem__(otuple{ slice_all, cutexpr });
}



static PKArgs args_cut(
  1, 2, 0, false, false,
  {
    "cols", "nbins", "right_closed"
  },
  "cut",

R"(cut(cols, nbins=10, right_closed=True)
--

Cut all the columns in a Frame/f-expression by binning
their values into discrete intervals.

Parameters
----------
cols: Frame | f-expression
    Frame or f-expression consisting of numeric columns.
nbins: int | list of ints | tuple of ints
    When a single number is specified, this number of bins
    will be used to bin each column of `cols`.
    When a list or a tuple is provided, each column will be binned
    by using its own number of bins. In the latter case,
    the list/tuple length must be equal to the number of columns
    in `cols`.
right_closed: bool
    Indicates whether bins should include the rightmost edge or not.

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
  oobj arg2 = args[2].is_none_or_undefined()? py::True() : args[2].to_oobj();

  if (arg0.is_frame()) {
    return cut_frame(arg0, arg1, arg2);
  }
  if (arg0.is_dtexpr()) {
    return make_pyexpr(
             dt::expr::Op::CUT,
             otuple{ arg0 },
             otuple{ arg1, arg2 }
           );
  }
  throw TypeError() << "The first argument to `cut()` must be a column "
      "expression or a Frame, instead got " << arg0.typeobj();
}



void DatatableModule::init_methods_cut() {
  ADD_FN(&py::pyfn_cut, py::args_cut);
}

}  // namespace py


