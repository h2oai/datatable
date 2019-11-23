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
#include "expr/fnary/fnary.h"
#include "expr/op.h"
#include "frame/py_frame.h"
#include "utils/assert.h"
#include "datatablemodule.h"


//------------------------------------------------------------------------------
// PKArgs -> Op
//------------------------------------------------------------------------------

static std::unordered_map<const py::PKArgs*, dt::expr::Op> args2opcodes;

static void register_args(const py::PKArgs& args, dt::expr::Op opcode) {
  xassert(args2opcodes.count(&args) == 0);
  args2opcodes[&args] = opcode;
}

static dt::expr::Op get_opcode_from_args(const py::PKArgs& args) {
  xassert(args2opcodes.count(&args) == 1);
  return args2opcodes.at(&args);
}




//------------------------------------------------------------------------------
// Main pyfn() function
//------------------------------------------------------------------------------

static py::oobj make_pyexpr(dt::expr::Op opcode, py::otuple args_tuple) {
  size_t op = static_cast<size_t>(opcode);
  return py::robj(py::Expr_Type).call({ py::oint(op), args_tuple });
}

static py::oobj make_pyexpr(dt::expr::Op opcode, py::otuple args_tuple,
                            py::otuple params_tuple)
{
  size_t op = static_cast<size_t>(opcode);
  return py::robj(py::Expr_Type)
            .call({ py::oint(op), args_tuple, params_tuple });
}

/**
  * This helper function will apply `opcode` to an entire frame.
  */
static py::oobj apply_to_frame(dt::expr::Op opcode, py::robj arg) {
  xassert(arg.is_frame());

  auto slice_all = py::oslice(py::oslice::NA, py::oslice::NA, py::oslice::NA);
  auto f_all = make_pyexpr(dt::expr::Op::COL,
                           py::otuple{ slice_all },
                           py::otuple{ py::oint(0) });
  auto rowfn = make_pyexpr(opcode, py::otuple{ f_all });

  auto frame = static_cast<py::Frame*>(arg.to_borrowed_ref());
  return frame->m__getitem__(py::otuple{ slice_all, rowfn });
}


/**
  * Python-facing function that implements the n-ary operator.
  *
  * All "rowwise" python functions are implemented using this
  * function, differentiating themselves only with the `args`
  * parameter.
  *
  * This function has two possible signatures: it can take either
  * a single Frame argument, in which case the rowwise function will
  * be immediately applied to the frame, and the resulting frame
  * returned; or it can take an Expr or sequence of Exprs as the
  * argument(s), and return a new Expr that encapsulates application
  * of the rowwise function to the given arguments.
  *
  */
static py::oobj fnary_pyfn(const py::PKArgs& args)
{
  auto opcode = get_opcode_from_args(args);
  size_t n = args.num_vararg_args();

  py::otuple expr_args(n);
  size_t i = 0;
  for (py::robj arg : args.varargs()) {
    if (n == 1 && arg.is_frame()) {
      return apply_to_frame(opcode, arg);
    }
    expr_args.set(i++, arg);
  }
  return make_pyexpr(opcode, expr_args);
}




//------------------------------------------------------------------------------
// Static initialization
//------------------------------------------------------------------------------

/**
  * Register python-facing functions. This is called once during
  * the initialization of `datatable` module.
  */
void py::DatatableModule::init_fnary()
{
  #define FNARY(ARGS, OP) \
    ADD_FN(&fnary_pyfn, dt::expr::ARGS); \
    register_args(dt::expr::ARGS, dt::expr::OP);

  FNARY(args_rowall,   Op::ROWALL);
  FNARY(args_rowany,   Op::ROWANY);
  FNARY(args_rowcount, Op::ROWCOUNT);
  FNARY(args_rowfirst, Op::ROWFIRST);
  FNARY(args_rowlast,  Op::ROWLAST);
  FNARY(args_rowmax,   Op::ROWMAX);
  FNARY(args_rowmean,  Op::ROWMEAN);
  FNARY(args_rowmin,   Op::ROWMIN);
  FNARY(args_rowsd,    Op::ROWSD);
  FNARY(args_rowsum,   Op::ROWSUM);
}
