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
#include "expr/funary/pyfn.h"
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

static py::oobj make_pyexpr(dt::expr::Op opcode, py::oobj arg) {
  size_t op = static_cast<size_t>(opcode);
  return py::robj(py::Expr_Type).call({ py::oint(op),
                                        py::otuple(arg) });
}

static py::oobj make_pyexpr(dt::expr::Op opcode, py::otuple args, py::otuple params) {
  size_t op = static_cast<size_t>(opcode);
  return py::robj(py::Expr_Type).call({ py::oint(op), args, params });
}


// This helper function will apply `opcode` to the entire frame, and
// return the resulting frame (same shape as the original).
static py::oobj process_frame(dt::expr::Op opcode, py::robj arg) {
  xassert(arg.is_frame());
  auto frame = static_cast<py::Frame*>(arg.to_borrowed_ref());
  DataTable* dt = frame->get_datatable();

  py::olist columns(dt->ncols());
  for (size_t i = 0; i < dt->ncols(); ++i) {
    py::oobj col_selector = make_pyexpr(dt::expr::Op::COL,
                                        py::otuple{py::oint(i)},
                                        py::otuple{py::oint(0)});
    columns.set(i, make_pyexpr(opcode, col_selector));
  }

  py::oobj res = frame->m__getitem__(py::otuple{ py::None(), columns });
  DataTable* res_dt = res.to_datatable();
  res_dt->copy_names_from(*dt);
  return res;
}

static py::oobj pyfn(const py::PKArgs& args)
{
  dt::expr::Op opcode = get_opcode_from_args(args);
  py::robj x = args[0].to_robj();
  if (x.is_dtexpr()) {
    return make_pyexpr(opcode, x);
  }
  if (x.is_frame()) {
    return process_frame(opcode, x);
  }
  // if (x.is_int()) {
  //   int64_t v = x.to_int64_strict();
  //   const uinfo* info = unary_library.get_infon(opcode, SType::INT64);
  //   if (info && info->output_stype == SType::INT64) {
  //     auto res = reinterpret_cast<int64_t(*)(int64_t)>(info->scalarfn)(v);
  //     return py::oint(res);
  //   }
  //   if (info && info->output_stype == SType::BOOL) {
  //     auto res = reinterpret_cast<bool(*)(int64_t)>(info->scalarfn)(v);
  //     return py::obool(res);
  //   }
  //   goto process_as_float;
  // }
  // if (x.is_float() || x.is_none()) {
  //   process_as_float:
  //   double v = x.to_double();
  //   const uinfo* info = unary_library.get_infon(opcode, SType::FLOAT64);
  //   if (info && info->output_stype == SType::FLOAT64) {
  //     auto res = reinterpret_cast<double(*)(double)>(info->scalarfn)(v);
  //     return py::ofloat(res);
  //   }
  //   if (info && info->output_stype == SType::BOOL) {
  //     auto res = reinterpret_cast<bool(*)(double)>(info->scalarfn)(v);
  //     return py::obool(res);
  //   }
  //   if (!x.is_none()) {
  //     throw TypeError() << "Function `" << args.get_short_name()
  //       << "` cannot be applied to a numeric argument";
  //   }
  //   // Fall-through if x is None
  // }
  if (x.is_string() || x.is_none()) {

  }
  if (!x) {
    throw TypeError() << "Function `" << args.get_short_name() << "()` takes "
        "exactly one argument, 0 given";
  }
  throw TypeError() << "Function `" << args.get_short_name() << "()` cannot "
      "be applied to an argument of type " << x.typeobj();
}




//------------------------------------------------------------------------------
// Static initialization
//------------------------------------------------------------------------------

/**
  * Register python-facing functions.
  */
void py::DatatableModule::init_funary()
{
  #define FUNARY(ARGS, OP) \
    ADD_FN(&pyfn, dt::expr::ARGS); \
    register_args(dt::expr::ARGS, dt::expr::OP);

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
}
