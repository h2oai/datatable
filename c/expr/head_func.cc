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
#include "expr/eval_context.h"
#include "expr/expr.h"
#include "expr/head_func.h"
#include "expr/head_func_other.h"
#include "expr/head_reduce.h"
#include "expr/workframe.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {



ptrHead Head_Func::from_op(Op op, const py::otuple& params) {
  size_t z = static_cast<size_t>(op);
  if (factory.count(z) == 0) {
    throw NotImplError() << "Unknown opcode in Expr(): " << z;
  }
  return factory[z](op, params);
}


Kind Head_Func::get_expr_kind() const {
  return Kind::Func;
}



// Forbid expressions like `f[f.A]`.
//
Workframe Head_Func::evaluate_f(EvalContext&, size_t, bool) const {
  throw TypeError() << "An expression cannot be used as a column selector";
}


// When used as j node, a Func expression means exactly the same as
// evaluating this expression in "normal" mode.
//
Workframe Head_Func::evaluate_j(
    const vecExpr& args, EvalContext& ctx, bool allow_new) const
{
  return evaluate_n(args, ctx, allow_new);
}


// When used as a replacement target, a Func expression behaves the
// same as during evaluation in "normal" mode.
//
Workframe Head_Func::evaluate_r(
    const vecExpr& args, EvalContext& ctx, const intvec&) const
{
  return evaluate_n(args, ctx, false);
}



RowIndex Head_Func::evaluate_i(const vecExpr& args, EvalContext& ctx) const {
  Workframe wf = evaluate_n(args, ctx, false);
  if (wf.ncols() != 1) {
    throw TypeError() << "i-expression evaluated into " << wf.ncols()
        << " columns";
  }
  Column col = wf.retrieve_column(0);
  if (col.stype() != SType::BOOL) {
    throw TypeError() << "Filter expression must be boolean, instead it "
        "was of type " << col.stype();
  }
  return RowIndex(std::move(col));
}


RiGb Head_Func::evaluate_iby(const vecExpr&, EvalContext&) const {
  throw NotImplError() << "Head_Func::evaluate_iby() not implemented yet";
}



//------------------------------------------------------------------------------
// Construction factory
//------------------------------------------------------------------------------

static ptrHead make_col(Op, const py::otuple& params) {
  xassert(params.size() == 1);
  size_t frame_id = params[0].to_size_t();
  return ptrHead(new Head_Func_Column(frame_id));
}


static ptrHead make_cast(Op, const py::otuple& params) {
  xassert(params.size() == 1);
  SType stype = params[0].to_stype();
  return ptrHead(new Head_Func_Cast(stype));
}


static ptrHead make_colsetop(Op op, const py::otuple& params) {
  xassert(params.size() == 0);
  return ptrHead(new Head_Func_Colset(op));
}

static ptrHead make_unop(Op op, const py::otuple& params) {
  xassert(params.size() == 0);
  return ptrHead(new Head_Func_Unary(op));
}


static ptrHead make_binop(Op op, const py::otuple& params) {
  xassert(params.size() == 0);
  return ptrHead(new Head_Func_Binary(op));
}


static ptrHead make_reduce0(Op op, const py::otuple& params) {
  xassert(params.size() == 0);
  return ptrHead(new Head_Reduce_Nullary(op));
}


static ptrHead make_reduce1(Op op, const py::otuple& params) {
  xassert(params.size() == 0);
  return ptrHead(new Head_Reduce_Unary(op));
}


static ptrHead make_reduce2(Op op, const py::otuple& params) {
  xassert(params.size() == 0);
  return ptrHead(new Head_Reduce_Binary(op));
}


static ptrHead make_rowfn(Op op, const py::otuple& params) {
  xassert(params.size() == 0);
  return ptrHead(new Head_Func_Nary(op));
}


std::unordered_map<size_t, Head_Func::maker_t> Head_Func::factory;

// static
void Head_Func::init() {
  for (size_t i = UNOP_FIRST;    i <= UNOP_LAST;    ++i) factory[i] = make_unop;
  for (size_t i = BINOP_FIRST;   i <= BINOP_LAST;   ++i) factory[i] = make_binop;
  for (size_t i = REDUCER_FIRST; i <= REDUCER_LAST; ++i) factory[i] = make_reduce1;
  for (size_t i = MATH_FIRST;    i <= MATH_LAST;    ++i) factory[i] = make_unop;
  for (size_t i = ROWFNS_FIRST;  i <= ROWFNS_LAST;  ++i) factory[i] = make_rowfn;
  factory[static_cast<size_t>(Op::COL)]        = make_col;
  factory[static_cast<size_t>(Op::CAST)]       = make_cast;
  factory[static_cast<size_t>(Op::SETPLUS)]    = make_colsetop;
  factory[static_cast<size_t>(Op::SETMINUS)]   = make_colsetop;
  factory[static_cast<size_t>(Op::SHIFTFN)]    = &Head_Func_Shift::make;
  factory[static_cast<size_t>(Op::COUNT0)]     = make_reduce0;
  factory[static_cast<size_t>(Op::COV)]        = make_reduce2;
  factory[static_cast<size_t>(Op::CORR)]       = make_reduce2;
  factory[static_cast<size_t>(Op::RE_MATCH)]   = &Head_Func_Re_Match::make;
  factory[static_cast<size_t>(Op::LEN)]        = make_unop;
  factory[static_cast<size_t>(Op::ARCTAN2)]    = make_binop;
  factory[static_cast<size_t>(Op::HYPOT)]      = make_binop;
  factory[static_cast<size_t>(Op::POWERFN)]    = make_binop;
  factory[static_cast<size_t>(Op::COPYSIGN)]   = make_binop;
  factory[static_cast<size_t>(Op::LOGADDEXP)]  = make_binop;
  factory[static_cast<size_t>(Op::LOGADDEXP2)] = make_binop;
  factory[static_cast<size_t>(Op::FMOD)]       = make_binop;
  factory[static_cast<size_t>(Op::LDEXP)]      = make_binop;
  factory[static_cast<size_t>(Op::ISCLOSE)]    = &Head_Func_IsClose::make;
}


}}  // namespace dt::expr
