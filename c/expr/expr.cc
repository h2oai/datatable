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
#include "expr/head.h"
#include "expr/head_frame.h"
#include "expr/head_func.h"
#include "expr/head_list.h"
#include "expr/head_literal.h"
#include "expr/workframe.h"
#include "expr/eval_context.h"
#include "datatable.h"
#include "datatablemodule.h"
namespace dt {
namespace expr {




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
  else if (src.is_range())         _init_from_range(src);
  else if (src.is_pandas_frame() ||
           src.is_pandas_series()) _init_from_pandas(src);
  else if (src.is_numpy_array() ||
           src.is_numpy_marray())  _init_from_numpy(src);
  else if (src.is_ellipsis())      _init_from_ellipsis();
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


void Expr::_init_from_ellipsis() {
  head = ptrHead(new Head_Literal_SliceAll);
}


void Expr::_init_from_float(py::robj src) {
  double x = src.to_double();
  head = ptrHead(new Head_Literal_Float(x));
}


void Expr::_init_from_frame(py::robj src) {
  head = Head_Frame::from_datatable(src);
}


void Expr::_init_from_int(py::robj src) {
  py::oint src_int = src.to_pyint();
  int overflow;
  int64_t x = src_int.ovalue<int64_t>(&overflow);
  if (overflow) {
    // If overflow occurs here, the returned value will be +/-Inf,
    // which is exactly what we need.
    double xx = src_int.ovalue<double>(&overflow);
    head = ptrHead(new Head_Literal_Float(xx));
  } else {
    head = ptrHead(new Head_Literal_Int(x));
  }
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


void Expr::_init_from_range(py::robj src) {
  py::orange rr = src.to_orange();
  head = ptrHead(new Head_Literal_Range(std::move(rr)));
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

Kind Expr::get_expr_kind() const {
  return head->get_expr_kind();
}

Expr::operator bool() const noexcept {
  return bool(head);
}


Workframe Expr::evaluate_n(EvalContext& ctx, bool allow_new) const {
  return head->evaluate_n(inputs, ctx, allow_new);
}


Workframe Expr::evaluate_j(EvalContext& ctx, bool allow_new) const
{
  return head->evaluate_j(inputs, ctx, allow_new);
}

Workframe Expr::evaluate_r(EvalContext& ctx, const intvec& indices) const
{
  return head->evaluate_r(inputs, ctx, indices);
}


Workframe Expr::evaluate_f(
    EvalContext& ctx, size_t frame_id, bool allow_new) const
{
  return head->evaluate_f(ctx, frame_id, allow_new);
}


RowIndex Expr::evaluate_i(EvalContext& ctx) const {
  return head->evaluate_i(inputs, ctx);
}


void Expr::prepare_by(EvalContext& ctx, Workframe& wf,
                      std::vector<SortFlag>& flags) const
{
  head->prepare_by(inputs, ctx, wf, flags);
}


RiGb Expr::evaluate_iby(EvalContext& ctx) const {
  return head->evaluate_iby(inputs, ctx);
}


bool Expr::evaluate_bool() const {
  auto boolhead = dynamic_cast<Head_Literal_Bool*>(head.get());
  xassert(boolhead);
  return boolhead->get_value();
}


bool Expr::is_negated_column(EvalContext& ctx, size_t* iframe,
                             size_t* icol) const
{
  auto unaryfn_head = dynamic_cast<Head_Func_Unary*>(head.get());
  if (unaryfn_head) {
    if (unaryfn_head->get_op() == Op::UMINUS) {
      xassert(inputs.size() == 1);
      auto column_head = dynamic_cast<Head_Func_Column*>(inputs[0].head.get());
      if (column_head) {
        Workframe wf = inputs[0].evaluate_n(ctx);
        xassert(wf.ncols() == 1);
        return wf.is_reference_column(0, iframe, icol);
      }
    }
  }
  return false;
}


int64_t Expr::evaluate_int() const {
  auto inthead = dynamic_cast<Head_Literal_Int*>(head.get());
  xassert(inthead);
  return inthead->get_value();
}




}}  // namespace dt::expr
