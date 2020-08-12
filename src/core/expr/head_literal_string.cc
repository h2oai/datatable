//------------------------------------------------------------------------------
// Copyright 2019-2020 H2O.ai
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
#include "column/const.h"
#include "expr/head_literal.h"
#include "expr/workframe.h"
#include "ltype.h"
namespace dt {
namespace expr {




Head_Literal_String::Head_Literal_String(py::robj x) : pystr(x) {}

Kind Head_Literal_String::get_expr_kind() const {
  return Kind::Str;
}


Workframe Head_Literal_String::evaluate_n(
    const vecExpr&, EvalContext& ctx) const
{
  return _wrap_column(ctx,
            Const_ColumnImpl::make_string_column(1, pystr.to_cstring()));
}


Workframe Head_Literal_String::evaluate_f(
    EvalContext& ctx, size_t frame_id) const
{
  auto df = ctx.get_datatable(frame_id);
  size_t j = df->xcolindex(pystr);
  Workframe outputs(ctx);
  outputs.add_ref_column(frame_id, j);
  return outputs;
}



// A string value is assigned to a DT[i,j] expression:
//
//   DT[:, j] = 'RESIST'
//
// The columns in `j` must be str32 or str64, and the replacement
// columns will try to match the stypes of the LHS.
//
Workframe Head_Literal_String::evaluate_r(
    const vecExpr&, EvalContext& ctx, const sztvec& indices) const
{
  auto dt0 = ctx.get_datatable(0);

  Workframe outputs(ctx);
  for (size_t i : indices) {
    SType stype;
    if (i < dt0->ncols()) {
      const Column& col = dt0->get_column(i);
      stype = (col.ltype() == LType::STRING)? col.stype() : SType::STR32;
    } else {
      stype = SType::STR32;
    }
    outputs.add_column(
        Const_ColumnImpl::make_string_column(1, pystr.to_cstring(), stype),
        std::string(),
        Grouping::SCALAR);
  }
  return outputs;
}



Workframe Head_Literal_String::evaluate_j(
    const vecExpr&, EvalContext& ctx) const
{
  auto df = ctx.get_datatable(0);
  Workframe outputs(ctx);
  if (ctx.get_mode() == EvalMode::UPDATE) {
    int64_t i = df->colindex(pystr);
    if (i < 0) outputs.add_placeholder(pystr.to_string(), 0);
    else       outputs.add_ref_column(0, static_cast<size_t>(i));
  }
  else {
    size_t j = df->xcolindex(pystr);
    outputs.add_ref_column(0, j);
  }
  return outputs;
}


RowIndex Head_Literal_String::evaluate_i(const vecExpr&, EvalContext&) const {
  throw TypeError() << "A string value cannot be used as a row selector";
}


RiGb Head_Literal_String::evaluate_iby(const vecExpr&, EvalContext&) const {
  throw TypeError() << "A string value cannot be used as a row selector";
}





}}  // namespace dt::expr
