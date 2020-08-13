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
#include "expr/eval_context.h"
#include "expr/fexpr_literal.h"
#include "expr/workframe.h"
#include "ltype.h"
#include "stype.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------

using stypevec = std::vector<SType>;

static stypevec stBOOL  = {SType::BOOL};
static stypevec stINT   = {SType::INT8, SType::INT16, SType::INT32, SType::INT64};
static stypevec stFLOAT = {SType::FLOAT32, SType::FLOAT64};
static stypevec stSTR   = {SType::STR32, SType::STR64};
static stypevec stTIME  = {};
static stypevec stOBJ   = {SType::OBJ};


static Workframe _select_types(
    EvalContext& ctx, size_t frame_id, const stypevec& stypes)
{
  const DataTable* df = ctx.get_datatable(frame_id);
  Workframe outputs(ctx);
  for (size_t i = 0; i < df->ncols(); ++i) {
    SType st = df->get_column(i).stype();
    for (SType s : stypes) {
      if (s == st) {
        outputs.add_ref_column(frame_id, i);
        break;
      }
    }
  }
  return outputs;
}


static Workframe _select_type(EvalContext& ctx, size_t frame_id, SType stype0)
{
  const DataTable* df = ctx.get_datatable(frame_id);
  Workframe outputs(ctx);
  for (size_t i = 0; i < df->ncols(); ++i) {
    SType stypei = df->get_column(i).stype();
    if (stypei == stype0) {
      outputs.add_ref_column(frame_id, i);
    }
  }
  return outputs;
}



//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

FExpr_Literal_Type::FExpr_Literal_Type(py::robj x)
  : value_(x) {}


ptrExpr FExpr_Literal_Type::make(py::robj src) {
  return ptrExpr(new FExpr_Literal_Type(src));
}




//------------------------------------------------------------------------------
// Evaluation
//------------------------------------------------------------------------------

Workframe FExpr_Literal_Type::evaluate_n(EvalContext&) const {
  throw TypeError() << value_ << " cannot appear in this context";
}


Workframe FExpr_Literal_Type::evaluate_f(EvalContext& ctx, size_t fid) const {
  if (value_.is_type()) {
    auto et = reinterpret_cast<PyTypeObject*>(value_.to_borrowed_ref());
    if (et == &PyLong_Type)       return _select_types(ctx, fid, stINT);
    if (et == &PyFloat_Type)      return _select_types(ctx, fid, stFLOAT);
    if (et == &PyUnicode_Type)    return _select_types(ctx, fid, stSTR);
    if (et == &PyBool_Type)       return _select_types(ctx, fid, stBOOL);
    if (et == &PyBaseObject_Type) return _select_types(ctx, fid, stOBJ);
  }
  if (value_.is_ltype()) {
    auto lt = static_cast<LType>(value_.get_attr("value").to_size_t());
    if (lt == LType::BOOL)     return _select_types(ctx, fid, stBOOL);
    if (lt == LType::INT)      return _select_types(ctx, fid, stINT);
    if (lt == LType::REAL)     return _select_types(ctx, fid, stFLOAT);
    if (lt == LType::STRING)   return _select_types(ctx, fid, stSTR);
    if (lt == LType::DATETIME) return _select_types(ctx, fid, stTIME);
    if (lt == LType::OBJECT)   return _select_types(ctx, fid, stOBJ);
  }
  if (value_.is_stype()) {
    auto st = static_cast<SType>(value_.get_attr("value").to_size_t());
    return _select_type(ctx, fid, st);
  }
  throw ValueError() << "Unknown type " << value_
                     << " used as a column selector";
}



Workframe FExpr_Literal_Type::evaluate_j(EvalContext& ctx) const {
  return evaluate_f(ctx, 0);
}


RowIndex FExpr_Literal_Type::evaluate_i(EvalContext&) const {
  throw TypeError() << "A type cannot be used as a row selector";
}


RiGb FExpr_Literal_Type::evaluate_iby(EvalContext&) const {
  throw TypeError() << "A type cannot be used as a row selector";
}


static void _resolve_stype(py::robj value_, SType* out_stype, LType* out_ltype)
{
  *out_stype = SType::VOID;
  *out_ltype = LType::MU;
  if (value_.is_type()) {
    auto et = reinterpret_cast<PyTypeObject*>(value_.to_borrowed_ref());
    *out_ltype = (et == &PyLong_Type)?       LType::INT :
                 (et == &PyFloat_Type)?      LType::REAL :
                 (et == &PyUnicode_Type)?    LType::STRING :
                 (et == &PyBool_Type)?       LType::BOOL :
                 (et == &PyBaseObject_Type)? LType::OBJECT :
                 LType::MU;
  }
  else if (value_.is_ltype()) {
    auto lt = value_.get_attr("value").to_size_t();
    *out_ltype = (lt < LTYPES_COUNT)? static_cast<LType>(lt) : LType::MU;
  }
  else if (value_.is_stype()) {
    auto st = value_.get_attr("value").to_size_t();
    *out_stype = (st < STYPES_COUNT)? static_cast<SType>(st) : SType::VOID;
  }
}

Workframe FExpr_Literal_Type::evaluate_r(
      EvalContext& ctx, const sztvec& indices) const
{
  if (ctx.get_rowindex(0)) {
    throw ValueError()
        << "Partial reassignment of Column's type is not possible";
  }
  SType target_stype;
  LType target_ltype;
  _resolve_stype(value_, &target_stype, &target_ltype);
  if (target_stype == SType::VOID && target_ltype == LType::MU) {
    throw ValueError() << "Unknown type " << value_
                       << " used in the replacement expression";
  }
  if (target_stype == SType::VOID) {
    target_stype = (target_ltype == LType::BOOL)? SType::BOOL :
                   (target_ltype == LType::INT)? SType::INT32 :
                   (target_ltype == LType::REAL)? SType::FLOAT64 :
                   (target_ltype == LType::STRING)? SType::STR32 :
                   (target_ltype == LType::OBJECT)? SType::OBJ : SType::VOID;
  }

  auto dt0 = ctx.get_datatable(0);
  Workframe res(ctx);
  for (size_t i : indices) {
    Column newcol;
    if (i < dt0->ncols()) {
      newcol = dt0->get_column(i);  // copy
      if (target_ltype != LType::MU) {
        if (newcol.ltype() != target_ltype) {
          newcol.cast_inplace(target_stype);
        }
      } else if (target_stype != SType::VOID) {
        newcol.cast_inplace(target_stype);
      }
    }
    else {
      newcol = Column::new_na_column(dt0->nrows(), target_stype);
    }

    res.add_column(std::move(newcol), "", Grouping::GtoALL);
  }
  return res;
}




//------------------------------------------------------------------------------
// Other methods
//------------------------------------------------------------------------------

Kind FExpr_Literal_Type::get_expr_kind() const {
  return Kind::Type;
}


int FExpr_Literal_Type::precedence() const noexcept {
  return 16;
}


std::string FExpr_Literal_Type::repr() const {
  if (value_.is_type()) {
    auto et = reinterpret_cast<PyTypeObject*>(value_.to_borrowed_ref());
    if (et == &PyLong_Type)       return "int";
    if (et == &PyFloat_Type)      return "float";
    if (et == &PyUnicode_Type)    return "str";
    if (et == &PyBool_Type)       return "bool";
    if (et == &PyBaseObject_Type) return "object";
  }
  if (value_.is_ltype()) {
    auto lt = static_cast<LType>(value_.get_attr("value").to_size_t());
    return std::string("ltype.") + ltype_name(lt);
  }
  if (value_.is_stype()) {
    auto st = static_cast<SType>(value_.get_attr("value").to_size_t());
    return std::string("stype.") + stype_name(st);
  }
  return value_.repr().to_string();
}




}}  // namespace dt::expr
