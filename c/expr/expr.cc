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
#include "expr/expr_literal.h"
#include "expr/expr_reduce.h"
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

bool base_expr::is_column_expr() const { return false; }

bool base_expr::is_negated_expr() const { return false; }

pexpr base_expr::get_negated_expr() { return pexpr(); }

size_t base_expr::get_col_index(const workframe&) { return size_t(-1); }




}}  // namespace dt::expr
//------------------------------------------------------------------------------
// py::base_expr
//------------------------------------------------------------------------------


// static void check_args_count(const std::vector<py::robj>& va, size_t n) {
//   if (va.size() == n) return;
//   throw TypeError() << "Expected " << n << " additional arguments, but "
//       "received " << va.size();
// }

// static std::unique_ptr<dt::base_expr> to_base_expr(const py::robj& arg) {
//   PyObject* v = arg.to_borrowed_ref();
//   if (Py_TYPE(v) != &py::base_expr::Type::type) {
//     throw TypeError()
//         << "Expected a base_expr object, but got " << arg.typeobj();
//   }
//   auto vv = reinterpret_cast<py::base_expr*>(v);
//   return vv->release();
// }


// void py::base_expr::m__init__(py::PKArgs& args) {
//   expr = nullptr;

//   size_t opcode = args[0].to_size_t();
//   std::vector<py::robj> va;
//   va.reserve(args.num_vararg_args());
//   for (auto item : args.varargs()) va.push_back(std::move(item));

//   switch (opcode) {
//     // case dt::exprCode::COL: {
//     //   check_args_count(va, 2);
//     //   expr = new dt::expr_column(va[0].to_size_t(), va[1]);
//     //   break;
//     // }
//     // case dt::exprCode::BINOP: {
//     //   check_args_count(va, 3);
//     //   size_t binop_code = va[0].to_size_t();
//     //   auto lhs = to_base_expr(va[1]);
//     //   auto rhs = to_base_expr(va[2]);
//     //   expr = new dt::expr_binaryop(binop_code, std::move(lhs), std::move(rhs));
//     //   break;
//     // }
//     // case dt::exprCode::LITERAL: {
//     //   check_args_count(va, 1);
//     //   expr = new dt::expr_literal(va[0]);
//     //   break;
//     // }
//     // case dt::exprCode::UNOP: {
//     //   check_args_count(va, 2);
//     //   size_t unop_code = va[0].to_size_t();
//     //   auto arg = to_base_expr(va[1]);
//     //   expr = new dt::expr_unaryop(unop_code, std::move(arg));
//     //   break;
//     // }
//     // case dt::exprCode::CAST: {
//     //   check_args_count(va, 2);
//     //   auto arg = to_base_expr(va[0]);
//     //   SType stype = static_cast<SType>(va[1].to_size_t());
//     //   expr = new dt::expr_cast(std::move(arg), stype);
//     //   break;
//     // }
//     // case dt::exprCode::UNREDUCE: {
//     //   check_args_count(va, 2);
//     //   size_t op = va[0].to_size_t();
//     //   auto arg = to_base_expr(va[1]);
//     //   expr = new expr::expr_reduce(std::move(arg), op);
//     //   break;
//     // }
//     // case dt::exprCode::NUREDUCE: {
//     //   check_args_count(va, 1);
//     //   size_t op = va[0].to_size_t();
//     //   xassert(op == 0);
//     //   expr = new dt::expr_reduce_nullary(op);
//     //   break;
//     // }
//     case dt::exprCode::STRINGFN: {
//       check_args_count(va, 3);
//       size_t op = va[0].to_size_t();
//       auto arg = to_base_expr(va[1]);
//       oobj params = va[2];
//       expr = dt::expr_string_fn(op, std::move(arg), params).release();
//       break;
//     }
//   }
// }




//------------------------------------------------------------------------------
// Factory function for creating `base_expr` objects from python
//------------------------------------------------------------------------------
using namespace dt::expr;

static void check_args_count(const py::otuple& args, size_t n) {
  if (args.size() == n) return;
  throw TypeError() << "Expected " << n << " parameters in Expr, but "
      "found " << args.size();
}


pexpr py::_obj::to_dtexpr() const {
  if (!is_dtexpr()) {
    return pexpr(new expr_literal(py::robj(v)));
  }
  const size_t op = get_attr("_op").to_size_t();
  const py::otuple args = get_attr("_args").to_otuple();

  if (op == static_cast<size_t>(Op::COL)) {
    check_args_count(args, 2);
    size_t frame_id = args[0].to_size_t();
    return pexpr(new expr_column(frame_id, args[1]));
  }
  if (op == static_cast<size_t>(Op::CAST)) {
    check_args_count(args, 2);
    pexpr arg = args[0].to_dtexpr();
    SType stype = args[1].to_stype();
    return pexpr(new expr_cast(std::move(arg), stype));
  }
  if (op == static_cast<size_t>(Op::COUNT0)) {
    check_args_count(args, 0);
    return pexpr(new expr_reduce0(op));
  }
  if (op >= UNOP_FIRST && op <= UNOP_LAST) {
    check_args_count(args, 1);
    pexpr arg = args[0].to_dtexpr();
    return pexpr(new expr_unaryop(std::move(arg), op));
  }
  if (op >= BINOP_FIRST && op <= BINOP_LAST) {
    check_args_count(args, 2);
    pexpr arg1 = args[0].to_dtexpr();
    pexpr arg2 = args[1].to_dtexpr();
    return pexpr(new expr_binaryop(std::move(arg1), std::move(arg2), op));
  }
  if (op >= REDUCER_FIRST && op <= REDUCER_LAST) {
    check_args_count(args, 1);
    pexpr arg = args[0].to_dtexpr();
    return pexpr(new expr_reduce1(std::move(arg), op));
  }
  throw ValueError() << "Invalid opcode in Expr(): " << op;
}
