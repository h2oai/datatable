//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
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
#include "column/latent.h"
#include "column/sentinel_fw.h"
#include "column/qcut.h"
#include "datatablemodule.h"
#include "expr/eval_context.h"
#include "expr/head_func.h"
#include "frame/py_frame.h"
#include "parallel/api.h"

namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// Head_Func_Qcut
//------------------------------------------------------------------------------

Head_Func_Qcut::Head_Func_Qcut(py::oobj py_nquantiles)
  : py_nquantiles_(py_nquantiles) {}


ptrHead Head_Func_Qcut::make(Op, const py::otuple& params) {
  xassert(params.size() == 1);
  return ptrHead(new Head_Func_Qcut(params[0]));
}



Workframe Head_Func_Qcut::evaluate_n(
    const vecExpr& args, EvalContext& ctx, bool) const
{

  if (ctx.has_groupby()) {
    throw NotImplError() << "qcut() cannot be used in a groupby context";
  }

  int32_t nquantiles_default = 10;
  Workframe wf = args[0].evaluate_n(ctx);
  const size_t ncols = wf.ncols();

  int32vec nquantiles(ncols);
  bool defined_nquantiles = !py_nquantiles_.is_none();
  bool nquantiles_list_or_tuple = py_nquantiles_.is_list_or_tuple();

  if (nquantiles_list_or_tuple) {
    py::oiter py_nquantiles = py_nquantiles_.to_oiter();
    if (py_nquantiles.size() != ncols) {
      throw ValueError() << "When `nquantiles` is a list or a tuple, its length must be "
        << "the same as the number of columns in the frame/expression, i.e. `"
        << ncols << "`, instead got: `" << py_nquantiles.size() << "`";

    }

    size_t i = 0;
    for (auto py_nquantile : py_nquantiles) {
      int32_t nquantile = py_nquantile.to_int32_strict();
      if (nquantile <= 0) {
        throw ValueError() << "All elements in `nquantiles` must be positive, "
                              "got `nquantiles[" << i << "`]: `" << nquantile << "`";
      }

      nquantiles[i++] = nquantile;
    }
    xassert(i == ncols);

  } else {
    if (defined_nquantiles) {
      nquantiles_default = py_nquantiles_.to_int32_strict();
      if (nquantiles_default <= 0) {
        throw ValueError() << "Number of quantiles must be positive, "
                              "instead got: `" << nquantiles_default << "`";
      }
    }

    for (size_t i = 0; i < ncols; ++i) {
      nquantiles[i] = nquantiles_default;
    }
  }

  // Qcut workframe in-place
  for (size_t i = 0; i < ncols; ++i) {
    Column coli = wf.retrieve_column(i);

    if (coli.ltype() == dt::LType::STRING || coli.ltype() == dt::LType::OBJECT)
    {
      throw TypeError() << "`qcut()` cannot be applied to "
        << "string or object columns, instead column `" << i
        << "` has an stype: `" << coli.stype() << "`";
    }

    coli = Column(new Latent_ColumnImpl(new Qcut_ColumnImpl(
             std::move(coli), nquantiles[i]
           )));
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


static const char* doc_qcut =
R"(qcut(cols, nquantiles=10)
--

Bin all the columns in a Frame/f-expression into equal-population
discrete intervals, i.e. quantiles. In reality, for some data
these quantiles may not have exactly the same population.

Parameters
----------
cols: Frame | f-expression
    Frame or f-expression for quantile binning.
nquantiles: int | list of ints | tuple of ints
    When a single number is specified, this number of quantiles
    will be used to bin each column in `cols`.
    When a list or a tuple is provided, each column will be binned
    by using its own number of quantiles. In the latter case,
    the list/tuple length must be equal to the number of columns
    in `cols`.

return: Frame | Expr
    Frame/f-expression, where each column is filled with
    the respective quantile ids.
)";

static PKArgs args_qcut(
  1, 0, 1, false, false,
  {
    "cols", "nquantiles"
  },
  "qcut", doc_qcut
);


/**
  * Python-facing function that can take either a Frame or
  * an f-expression as an argument.
  */
static oobj pyfn_qcut(const PKArgs& args)
{
  if (args[0].is_none_or_undefined()) {
    throw TypeError() << "Function `qcut()` requires one positional argument, "
                         "but none were given";
  }


  oobj arg0 = args[0].to_oobj();
  oobj arg1 = args[1].is_none_or_undefined()? py::None() : args[1].to_oobj();

  return make_pyexpr(dt::expr::Op::QCUT,
                     otuple{ arg0 },
                     otuple{ arg1 });
}



void DatatableModule::init_methods_qcut() {
  ADD_FN(&py::pyfn_qcut, py::args_qcut);
}

}  // namespace py


