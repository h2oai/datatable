//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#include <memory>             // std::unique_ptr
#include <stdlib.h>
#include "expr/expr.h"
#include "expr/expr_binaryop.h"
#include "expr/expr_cast.h"
#include "expr/expr_column.h"
#include "expr/expr_columnset.h"
#include "expr/expr_literal.h"
#include "expr/expr_reduce.h"
#include "expr/expr_str.h"
#include "expr/expr_unaryop.h"
#include "expr/workframe.h"
#include "datatable.h"
#include "datatablemodule.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// base_expr
//------------------------------------------------------------------------------

base_expr::base_expr() {
  TRACK(this, sizeof(*this), "dt::base_expr");
}

base_expr::~base_expr() {
  UNTRACK(this);
}

bool base_expr::is_columnset_expr() const { return false; }
bool base_expr::is_literal_expr() const { return false; }

bool base_expr::is_negated_expr() const { return false; }

pexpr base_expr::get_negated_expr() { return pexpr(); }

py::oobj base_expr::get_literal_arg() { return py::oobj(); }

vcolptr base_expr::evaluate_lazy(workframe& wf) {
  return virtualize(evaluate_eager(wf));
}




//------------------------------------------------------------------------------
// Factory function for creating `base_expr` objects from python
//------------------------------------------------------------------------------

static void check_args_count(const py::otuple& args, size_t n) {
  if (args.size() == n) return;
  throw TypeError() << "Expected " << n << " parameters in Expr, but "
      "found " << args.size();
}


static pexpr make_col(Op, const py::otuple& args) {
  check_args_count(args, 2);
  size_t frame_id = args[0].to_size_t();
  if (args[1].is_int() || args[1].is_string()) {
    return pexpr(new expr_column(frame_id, args[1]));
  } else {
    return pexpr(new expr_simple_columnset(frame_id, args[1]));
  }
}

static pexpr make_cast(Op, const py::otuple& args) {
  check_args_count(args, 2);
  pexpr arg = args[0].to_dtexpr();
  SType stype = args[1].to_stype();
  return pexpr(new expr_cast(std::move(arg), stype));
}

static pexpr make_count0(Op op, const py::otuple& args) {
  check_args_count(args, 0);
  return pexpr(new expr_reduce0(op));
}

static pexpr make_unop(Op op, const py::otuple& args) {
  check_args_count(args, 1);
  pexpr arg = args[0].to_dtexpr();
  return pexpr(new expr_unaryop(std::move(arg), op));
}


static pexpr make_binop(Op op, const py::otuple& args) {
  check_args_count(args, 2);
  pexpr arg1 = args[0].to_dtexpr();
  pexpr arg2 = args[1].to_dtexpr();
  return pexpr(new expr_binaryop(std::move(arg1), std::move(arg2), op));
}


static pexpr make_reduce(Op op, const py::otuple& args) {
  check_args_count(args, 1);
  pexpr arg = args[0].to_dtexpr();
  return pexpr(new expr_reduce1(std::move(arg), op));
}


static pexpr make_string(Op, const py::otuple& args) {
  check_args_count(args, 2);
  pexpr arg = args[0].to_dtexpr();
  py::oobj params = args[1];
  return pexpr(new expr_string_match_re(std::move(arg), params));
}


static pexpr make_setplus(Op, const py::otuple& args) {
  check_args_count(args, 2);
  pexpr arg1 = args[0].to_dtexpr();
  pexpr arg2 = args[1].to_dtexpr();
  return pexpr(new expr_sum_columnset(std::move(arg1), std::move(arg2)));
}


static pexpr make_setminus(Op, const py::otuple& args) {
  check_args_count(args, 2);
  pexpr arg1 = args[0].to_dtexpr();
  pexpr arg2 = args[1].to_dtexpr();
  return pexpr(new expr_diff_columnset(std::move(arg1), std::move(arg2)));
}


using expr_maker_t = pexpr(*)(Op, const py::otuple&);
static std::unordered_map<size_t, expr_maker_t> factory;

void init_expr() {
  for (size_t i = UNOP_FIRST;    i <= UNOP_LAST;    ++i) factory[i] = make_unop;
  for (size_t i = BINOP_FIRST;   i <= BINOP_LAST;   ++i) factory[i] = make_binop;
  for (size_t i = REDUCER_FIRST; i <= REDUCER_LAST; ++i) factory[i] = make_reduce;
  for (size_t i = MATH_FIRST;    i <= MATH_LAST;    ++i) factory[i] = make_unop;
  factory[static_cast<size_t>(Op::COL)]      = make_col;
  factory[static_cast<size_t>(Op::CAST)]     = make_cast;
  factory[static_cast<size_t>(Op::SETPLUS)]  = make_setplus;
  factory[static_cast<size_t>(Op::SETMINUS)] = make_setminus;
  factory[static_cast<size_t>(Op::COUNT0)]   = make_count0;
  factory[static_cast<size_t>(Op::RE_MATCH)] = make_string;
  // factory[static_cast<size_t>(Op::HYPOT)]    = make_math21;
  // factory[static_cast<size_t>(Op::ARCTAN2)]  = make_math21;
  // factory[static_cast<size_t>(Op::POWER)]    = make_math21;
  // factory[static_cast<size_t>(Op::COPYSIGN)] = make_math21;
}



py::oobj make_pyexpr(Op opcode, py::oobj arg) {
  size_t op = static_cast<size_t>(opcode);
  return py::robj(py::Expr_Type).call({ py::oint(op), arg });
}


py::oobj make_pyexpr(Op opcode, py::oobj arg1, py::oobj arg2) {
  size_t op = static_cast<size_t>(opcode);
  return py::robj(py::Expr_Type).call({ py::oint(op), arg1, arg2 });
}




}}  // namespace dt::expr


using namespace dt::expr;
pexpr py::_obj::to_dtexpr() const {
  if (!is_dtexpr()) {
    return pexpr(new expr_literal(py::robj(v)));
  }
  const size_t op = get_attr("_op").to_size_t();
  const py::otuple args = get_attr("_args").to_otuple();

  if (factory.count(op) == 0) {
    throw ValueError() << "Unknown opcode in Expr(): " << op;
  }
  return factory[op](static_cast<Op>(op), args);
}
