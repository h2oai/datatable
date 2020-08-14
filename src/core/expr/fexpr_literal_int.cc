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
#include "expr/eval_context.h"
#include "expr/fexpr_literal.h"
#include "expr/workframe.h"
#include "ltype.h"
#include "python/float.h"    // tmp
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

FExpr_Literal_Int::FExpr_Literal_Int(int64_t x)
  : value_(x) {}


ptrExpr FExpr_Literal_Int::make(py::robj src) {
  py::oint src_int = src.to_pyint();
  int overflow;
  int64_t x = src_int.ovalue<int64_t>(&overflow);
  if (overflow) {
    // If overflow occurs here, the returned value will be +/-Inf,
    // which is exactly what we need.
    double xx = src_int.ovalue<double>(&overflow);
    return ptrExpr(new FExpr_Literal_Float(xx));
  } else {
    return ptrExpr(new FExpr_Literal_Int(x));
  }
}



//------------------------------------------------------------------------------
// Evaluators
//------------------------------------------------------------------------------

Workframe FExpr_Literal_Int::evaluate_n(EvalContext& ctx) const {
  return Workframe(ctx, Const_ColumnImpl::make_int_column(1, value_));
}


Workframe FExpr_Literal_Int::evaluate_f(EvalContext& ctx, size_t ns) const
{
  auto df = ctx.get_datatable(ns);
  Workframe outputs(ctx);
  int64_t icols = static_cast<int64_t>(df->ncols());
  if (value_ < -icols || value_ >= icols) {
      throw ValueError()
          << "Column index `" << value_ << "` is invalid for a Frame with "
          << icols << " column" << (icols == 1? "" : "s");
  } else {
    size_t i = static_cast<size_t>(value_ < 0? value_ + icols : value_);
    outputs.add_ref_column(ns, i);
  }
  return outputs;
}



Workframe FExpr_Literal_Int::evaluate_j(EvalContext& ctx) const
{
  return evaluate_f(ctx, 0);
}



// An integer value is assigned to a DT[i,j] expression:
//
//   DT[:, j] = -1
//
// This is allowed provided that the columns in `j` are either
// integer or float.
//
Workframe FExpr_Literal_Int::evaluate_r(
    EvalContext& ctx, const sztvec& indices) const
{
  auto dt0 = ctx.get_datatable(0);

  Workframe outputs(ctx);
  for (size_t i : indices) {
    Column newcol;
    if (i < dt0->ncols()) {
      const Column& col = dt0->get_column(i);
      LType ltype = col.ltype();
      if (ltype == LType::INT) {
        // This creates a column with the requested `stype`, but only
        // if the `value` fits inside the range of that stype. If not,
        // the column will be auto-promoted to the next smallest integer
        // stype.
        newcol = Const_ColumnImpl::make_int_column(1, value_, col.stype());
      }
      else if (ltype == LType::REAL) {
        newcol = Const_ColumnImpl::make_float_column(
                   1,
                   static_cast<double>(value_),
                   col.stype()
                 );
      }
      else {
        newcol = Const_ColumnImpl::make_int_column(1, value_);
      }
    } else {
      newcol = Const_ColumnImpl::make_int_column(1, value_);
    }
    outputs.add_column(std::move(newcol), std::string(), Grouping::SCALAR);
  }
  return outputs;
}



RowIndex FExpr_Literal_Int::evaluate_i(EvalContext& ctx) const {
  int64_t inrows = static_cast<int64_t>(ctx.nrows());
  if (value_ < -inrows || value_ >= inrows) {
    throw ValueError() << "Row `" << value_ << "` is invalid for a frame with "
        << inrows << " row" << (inrows == 1? "" : "s");
  }
  auto irow = static_cast<size_t>((value_ >= 0)? value_ : value_ + inrows);
  return RowIndex(irow, 1, 1);
}


RiGb FExpr_Literal_Int::evaluate_iby(EvalContext& ctx) const {
  const int32_t ivalue = static_cast<int32_t>(value_);
  if (ivalue != value_) {
    return RiGb{ RowIndex(Buffer(), RowIndex::ARR32),
                 Groupby::zero_groups() };
  }

  const Groupby& inp_groupby = ctx.get_groupby();
  const int32_t* inp_group_offsets = inp_groupby.offsets_r();
  size_t ngroups = inp_groupby.size();

  Buffer out_ri_buffer = Buffer::mem(ngroups * sizeof(int32_t));
  int32_t* out_rowindices = static_cast<int32_t*>(out_ri_buffer.xptr());

  int32_t k = 0;  // index for output groups
  if (ivalue >= 0) {
    for (size_t g = 0; g < ngroups; ++g) {
      int32_t group_start = inp_group_offsets[g];
      int32_t group_end   = inp_group_offsets[g + 1];
      int32_t group_ith   = group_start + ivalue;
      if (group_ith < group_end) {
        out_rowindices[k++] = group_ith;
      }
    }
  } else {
    for (size_t g = 0; g < ngroups; ++g) {
      int32_t group_start = inp_group_offsets[g];
      int32_t group_end   = inp_group_offsets[g + 1];
      int32_t group_ith = group_end + ivalue;
      if (group_ith >= group_start) {
        out_rowindices[k++] = group_ith;
      }
    }
  }

  size_t zk = static_cast<size_t>(k);
  Buffer out_groups = Buffer::mem((zk + 1) * sizeof(int32_t));
  int32_t* out_group_offsets = static_cast<int32_t*>(out_groups.xptr());
  for (int32_t i = 0; i <= k; i++) {
    out_group_offsets[i] = i;
  }

  out_ri_buffer.resize(zk * sizeof(int32_t));
  return RiGb{ RowIndex(std::move(out_ri_buffer),
                        RowIndex::ARR32|RowIndex::SORTED),
               Groupby(zk, std::move(out_groups)) };
}



//------------------------------------------------------------------------------
// Other methods
//------------------------------------------------------------------------------

Kind FExpr_Literal_Int::get_expr_kind() const {
  return Kind::Int;
}

int64_t FExpr_Literal_Int::evaluate_int() const {
  return value_;
}


int FExpr_Literal_Int::precedence() const noexcept {
  return 18;
}


std::string FExpr_Literal_Int::repr() const {
  return std::to_string(value_);
}






}}  // namespace dt::expr
