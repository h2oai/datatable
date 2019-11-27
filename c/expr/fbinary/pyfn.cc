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
#include "expr/fbinary/bimaker.h"
#include "expr/args_registry.h"
#include "expr/op.h"
#include "frame/py_frame.h"
#include "utils/assert.h"
#include "datatablemodule.h"
namespace dt {
namespace expr {




//------------------------------------------------------------------------------
// Main pyfn() function
//------------------------------------------------------------------------------

static py::oobj make_pyexpr(dt::expr::Op opcode, py::otuple args_tuple,
                            py::otuple params_tuple)
{
  size_t op = static_cast<size_t>(opcode);
  return py::robj(py::Expr_Type)
            .call({ py::oint(op), args_tuple, params_tuple });
}


/**
  * Python-facing function that implements the binary operators.
  *
  *
  */
static py::oobj fbinary_pyfn(const py::PKArgs& args)
{
  Op opcode = get_opcode_from_args(args);

  py::robj x = args[0].to_robj();
  py::robj y = args[1].to_robj();
  py::otuple params(0);
  if (!x || !y) {
    throw TypeError() << "Function `" << args.get_short_name() << "()` takes "
        "2 positional arguments";
  }

  return make_pyexpr(opcode,
                     py::otuple{ x, y },
                     std::move(params));
}




}}  // namespace dt::expr
//------------------------------------------------------------------------------
// Static initialization
//------------------------------------------------------------------------------

/**
  * Register python-facing functions. This is called once during
  * the initialization of `datatable` module.
  */
void py::DatatableModule::init_fbinary()
{
  #define FBINARY(ARGS, OP) \
    ADD_FN(&dt::expr::fbinary_pyfn, dt::expr::ARGS); \
    register_args(dt::expr::ARGS, dt::expr::OP);

  FBINARY(args_atan2,      Op::ARCTAN2);
  FBINARY(args_hypot,      Op::HYPOT);
  FBINARY(args_pow,        Op::POWERFN);
  FBINARY(args_copysign,   Op::COPYSIGN);
  FBINARY(args_logaddexp,  Op::LOGADDEXP);
  FBINARY(args_logaddexp2, Op::LOGADDEXP2);
  FBINARY(args_fmod,       Op::FMOD);
  FBINARY(args_ldexp,      Op::LDEXP);
}
