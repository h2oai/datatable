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
#include "expr/expr.h"
#include "expr/expr_binaryop.h"
#include "expr/expr_cast.h"
#include "expr/expr_column.h"
#include "expr/expr_columnset.h"
#include "expr/expr_literal.h"
#include "expr/expr_reduce.h"
#include "expr/expr_str.h"
#include "expr/expr_unaryop.h"
#include "expr/head.h"
#include "expr/head_frame.h"
#include "expr/head_func.h"
#include "expr/head_list.h"
#include "expr/head_literal.h"
#include "expr/outputs.h"
#include "expr/workframe.h"
#include "datatable.h"
#include "datatablemodule.h"
namespace dt {
namespace expr {


Head::~Head() {}


//------------------------------------------------------------------------------
// Expr construction
//------------------------------------------------------------------------------

Expr::Expr(py::robj src) {
  if      (src.is_dtexpr())        _init_from_dtexpr(src);
  else if (src.is_int())           _init_from_int(src);
  else if (src.is_string())        _init_from_string(src);
  else if (src.is_float())         _init_from_float(src);
  else if (src.is_bool())          _init_from_bool(src);
  else if (src.is_slice())         _init_from_slice(src);
  else if (src.is_list_or_tuple()) _init_from_list(src);
  else if (src.is_dict())          _init_from_dictionary(src);
  else if (src.is_anytype())       _init_from_type(src);
  else if (src.is_generator())     _init_from_iterable(src);
  else if (src.is_none())          _init_from_none();
  else if (src.is_frame())         _init_from_frame(src);
  else if (src.is_pandas_frame() ||
           src.is_pandas_series()) _init_from_pandas(src);
  else if (src.is_numpy_array() ||
           src.is_numpy_marray())  _init_from_numpy(src);
  else {
    throw TypeError() << "An object of type " << src.typeobj()
                      << " cannot be used in an Expr";
  }
}


void Expr::_init_from_bool(py::robj src) {
  int8_t t = src.to_bool_strict();
  head = ptrHead(new Head_Literal_Bool(t));
}


void Expr::_init_from_dictionary(py::robj src) {
  strvec names;
  for (auto kv : src.to_pydict()) {
    if (!kv.first.is_string()) {
      throw TypeError() << "Keys in the dictionary must be strings";
    }
    names.push_back(kv.first.to_string());
    inputs.emplace_back(kv.second);
  }
  head = ptrHead(new Head_NamedList(std::move(names)));
}


void Expr::_init_from_dtexpr(py::robj src) {
  auto op     = src.get_attr("_op").to_size_t();
  auto args   = src.get_attr("_args").to_otuple();
  auto params = src.get_attr("_params").to_otuple();

  for (size_t i = 0; i < args.size(); ++i) {
    inputs.emplace_back(args[i]);
  }
  head = Head_Func::from_op(static_cast<Op>(op), params);
}


void Expr::_init_from_float(py::robj src) {
  double x = src.to_double();
  head = ptrHead(new Head_Literal_Float(x));
}


void Expr::_init_from_frame(py::robj src) {
  head = Head_Frame::from_datatable(src);
}


void Expr::_init_from_int(py::robj src) {
  int64_t x = src.to_int64_strict();
  head = ptrHead(new Head_Literal_Int(x));
}


void Expr::_init_from_iterable(py::robj src) {
  for (auto elem : src.to_oiter()) {
    inputs.emplace_back(elem);
  }
  head = ptrHead(new Head_List);
}


void Expr::_init_from_list(py::robj src) {
  auto srclist = src.to_pylist();
  size_t nelems = srclist.size();
  for (size_t i = 0; i < nelems; ++i) {
    inputs.emplace_back(srclist[i]);
  }
  head = ptrHead(new Head_List);
}


void Expr::_init_from_none() {
  head = ptrHead(new Head_Literal_None);
}


void Expr::_init_from_numpy(py::robj src) {
  head = Head_Frame::from_numpy(src);
}


void Expr::_init_from_pandas(py::robj src) {
  head = Head_Frame::from_pandas(src);
}


void Expr::_init_from_slice(py::robj src) {
  auto src_as_slice = src.to_oslice();
  if (src_as_slice.is_trivial()) {
    head = ptrHead(new Head_Literal_SliceAll);
  }
  else if (src_as_slice.is_numeric()) {
    head = ptrHead(new Head_Literal_SliceInt(src_as_slice));
  }
  else if (src_as_slice.is_string()) {
    head = ptrHead(new Head_Literal_SliceStr(src_as_slice));
  }
  else {
    throw TypeError() << src << " is neither integer- nor string- valued";
  }
}


void Expr::_init_from_string(py::robj src) {
  head = ptrHead(new Head_Literal_String(src));
}


void Expr::_init_from_type(py::robj src) {
  head = ptrHead(new Head_Literal_Type(src));
}




//------------------------------------------------------------------------------
// Expr core functionality
//------------------------------------------------------------------------------

Outputs Expr::evaluate(workframe& wf) const {
  return head->evaluate(inputs, wf);
}













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





//------------------------------------------------------------------------------
// Factory function for creating `base_expr` objects from python
//------------------------------------------------------------------------------

static void check_args_count(const py::otuple& args, size_t n) {
  if (args.size() == n) return;
  throw TypeError() << "Expected " << n << " parameters in Expr, but "
      "found " << args.size();
}


static pexpr make_col(Op, const py::otuple&, const py::otuple& params) {
  check_args_count(params, 2);
  size_t frame_id = params[0].to_size_t();
  if (params[1].is_int() || params[1].is_string()) {
    return pexpr(new expr_column(frame_id, params[1]));
  } else {
    return pexpr(new expr_simple_columnset(frame_id, params[1]));
  }
}

static pexpr make_cast(Op, const py::otuple& args, const py::otuple& params) {
  check_args_count(args, 1);
  check_args_count(params, 1);
  pexpr arg = args[0].to_dtexpr();
  SType stype = params[0].to_stype();
  return pexpr(new expr_cast(std::move(arg), stype));
}

static pexpr make_count0(Op op, const py::otuple& args, const py::otuple&) {
  check_args_count(args, 0);
  return pexpr(new expr_reduce0(op));
}

static pexpr make_unop(Op op, const py::otuple& args, const py::otuple&) {
  check_args_count(args, 1);
  pexpr arg = args[0].to_dtexpr();
  return pexpr(new expr_unaryop(std::move(arg), op));
}


static pexpr make_binop(Op op, const py::otuple& args, const py::otuple&) {
  check_args_count(args, 2);
  pexpr arg1 = args[0].to_dtexpr();
  pexpr arg2 = args[1].to_dtexpr();
  return pexpr(new expr_binaryop(std::move(arg1), std::move(arg2), op));
}


static pexpr make_reduce(Op op, const py::otuple& args, const py::otuple&) {
  check_args_count(args, 1);
  pexpr arg = args[0].to_dtexpr();
  return pexpr(new expr_reduce1(std::move(arg), op));
}


static pexpr make_string(Op, const py::otuple& args, const py::otuple& params) {
  check_args_count(args, 1);
  pexpr arg = args[0].to_dtexpr();
  return pexpr(new expr_string_match_re(std::move(arg), params));
}


static pexpr make_setplus(Op, const py::otuple& args, const py::otuple&) {
  check_args_count(args, 2);
  pexpr arg1 = args[0].to_dtexpr();
  pexpr arg2 = args[1].to_dtexpr();
  return pexpr(new expr_sum_columnset(std::move(arg1), std::move(arg2)));
}


static pexpr make_setminus(Op, const py::otuple& args, const py::otuple&) {
  check_args_count(args, 2);
  pexpr arg1 = args[0].to_dtexpr();
  pexpr arg2 = args[1].to_dtexpr();
  return pexpr(new expr_diff_columnset(std::move(arg1), std::move(arg2)));
}


using expr_maker_t = pexpr(*)(Op, const py::otuple&, const py::otuple&);
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
  return py::robj(py::Expr_Type).call({ py::oint(op),
                                        py::otuple(arg) });
}


py::oobj make_pyexpr(Op opcode, py::otuple args, py::otuple params) {
  size_t op = static_cast<size_t>(opcode);
  return py::robj(py::Expr_Type).call({ py::oint(op), args, params });
}




}}  // namespace dt::expr


using namespace dt::expr;
pexpr py::_obj::to_dtexpr() const {
  if (!is_dtexpr()) {
    return pexpr(new expr_literal(py::robj(v)));
  }
  size_t op = get_attr("_op").to_size_t();
  py::otuple args = get_attr("_args").to_otuple();
  py::otuple params = get_attr("_params").to_otuple();

  if (factory.count(op) == 0) {
    throw ValueError() << "Unknown opcode in Expr(): " << op;
  }
  return factory[op](static_cast<Op>(op), args, params);
}
