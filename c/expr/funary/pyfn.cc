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
#include "expr/funary/pyfn.h"
#include "expr/funary/umaker.h"
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

static py::oobj make_pyexpr(Op opcode, py::oobj arg) {
  size_t op = static_cast<size_t>(opcode);
  return py::robj(py::Expr_Type).call({ py::oint(op),
                                        py::otuple(arg) });
}

static py::oobj make_pyexpr(Op opcode, py::otuple args, py::otuple params) {
  size_t op = static_cast<size_t>(opcode);
  return py::robj(py::Expr_Type).call({ py::oint(op), args, params });
}


// This helper function will apply `opcode` to the entire frame, and
// return the resulting frame (same shape as the original).
static py::oobj process_frame(Op opcode, py::robj arg) {
  xassert(arg.is_frame());
  auto frame = static_cast<py::Frame*>(arg.to_borrowed_ref());
  DataTable* dt = frame->get_datatable();

  py::olist columns(dt->ncols());
  for (size_t i = 0; i < dt->ncols(); ++i) {
    py::oobj col_selector = make_pyexpr(Op::COL,
                                        py::otuple{py::oint(i)},
                                        py::otuple{py::oint(0)});
    columns.set(i, make_pyexpr(opcode, col_selector));
  }

  py::oobj res = frame->m__getitem__(py::otuple{ py::None(), columns });
  DataTable* res_dt = res.to_datatable();
  res_dt->copy_names_from(*dt);
  return res;
}


/**
  * Python-facing function that implements an unary operator / single-
  * argument function. This function can take as an argument either a
  * python scalar, or an f-expression, or a Frame (in which case the
  * function is applied to all elements of the frame).
  */
static py::oobj funary_pyfn(const py::PKArgs& args)
{
  Op opcode = get_opcode_from_args(args);
  py::robj x = args[0].to_robj();
  if (x.is_dtexpr()) {
    return make_pyexpr(opcode, x);
  }
  if (x.is_frame()) {
    return process_frame(opcode, x);
  }
  if (x.is_int())   return unaryop(opcode, x.to_int64_strict());
  if (x.is_float()) return unaryop(opcode, x.to_double());
  if (x.is_none())  return unaryop(opcode, nullptr);
  if (x.is_bool())  return unaryop(opcode, static_cast<bool>(x.to_bool_strict()));
  if (x.is_string())return unaryop(opcode, x.to_cstring());
  if (!x) {
    throw TypeError() << "Function `" << args.get_short_name() << "()` takes "
        "exactly one argument, 0 given";
  }
  throw TypeError() << "Function `" << args.get_short_name() << "()` cannot "
      "be applied to an argument of type " << x.typeobj();
}




}}  // namespace dt::expr
//------------------------------------------------------------------------------
// Static initialization
//------------------------------------------------------------------------------

/**
  * Register python-facing functions.
  */
void py::DatatableModule::init_funary()
{
  #define FUNARY(ARGS, OP) \
    ADD_FN(&dt::expr::funary_pyfn, dt::expr::ARGS); \
    register_args(dt::expr::ARGS, dt::expr::OP);

  // Basic
  FUNARY(args_len,     Op::LEN);

  // Trigonometric
  FUNARY(args_sin,     Op::SIN);
  FUNARY(args_cos,     Op::COS);
  FUNARY(args_tan,     Op::TAN);
  FUNARY(args_arcsin,  Op::ARCSIN);
  FUNARY(args_arccos,  Op::ARCCOS);
  FUNARY(args_arctan,  Op::ARCTAN);
  FUNARY(args_deg2rad, Op::DEG2RAD);
  FUNARY(args_rad2deg, Op::RAD2DEG);

  // Hyperbolic
  FUNARY(args_sinh,    Op::SINH);
  FUNARY(args_cosh,    Op::COSH);
  FUNARY(args_tanh,    Op::TANH);
  FUNARY(args_arsinh,  Op::ARSINH);
  FUNARY(args_arcosh,  Op::ARCOSH);
  FUNARY(args_artanh,  Op::ARTANH);

  // Exponential/power
  FUNARY(args_cbrt,    Op::CBRT);
  FUNARY(args_exp,     Op::EXP);
  FUNARY(args_exp2,    Op::EXP2);
  FUNARY(args_expm1,   Op::EXPM1);
  FUNARY(args_log,     Op::LOG);
  FUNARY(args_log10,   Op::LOG10);
  FUNARY(args_log1p,   Op::LOG1P);
  FUNARY(args_log2,    Op::LOG2);
  FUNARY(args_sqrt,    Op::SQRT);
  FUNARY(args_square,  Op::SQUARE);

  // Special
  FUNARY(args_erf,     Op::ERF);
  FUNARY(args_erfc,    Op::ERFC);
  FUNARY(args_gamma,   Op::GAMMA);
  FUNARY(args_lgamma,  Op::LGAMMA);

  // Floating-point
  FUNARY(args_isfinite,  Op::ISFINITE);
  FUNARY(args_isinf,     Op::ISINF);
  FUNARY(args_isna,      Op::ISNA);
  FUNARY(args_ceil,      Op::CEIL);
  FUNARY(args_abs,       Op::ABS);
  FUNARY(args_fabs,      Op::FABS);
  FUNARY(args_floor,     Op::FLOOR);
  FUNARY(args_rint,      Op::RINT);
  FUNARY(args_sign,      Op::SIGN);
  FUNARY(args_signbit,   Op::SIGNBIT);
  FUNARY(args_trunc,     Op::TRUNC);
}
