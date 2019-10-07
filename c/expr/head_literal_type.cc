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
#include "expr/head_literal.h"
#include "expr/workframe.h"
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
// Head_Literal_Type
//------------------------------------------------------------------------------

Head_Literal_Type::Head_Literal_Type(py::robj x) : value(x) {}

Kind Head_Literal_Type::get_expr_kind() const {
  return Kind::Type;
}


Workframe Head_Literal_Type::evaluate_n(const vecExpr&, EvalContext&) const {
  throw TypeError() << value << " cannot appear in this context";
}


Workframe Head_Literal_Type::evaluate_f(
    EvalContext& ctx, size_t fid, bool) const
{
  if (value.is_type()) {
    auto et = reinterpret_cast<PyTypeObject*>(value.to_borrowed_ref());
    if (et == &PyLong_Type)       return _select_types(ctx, fid, stINT);
    if (et == &PyFloat_Type)      return _select_types(ctx, fid, stFLOAT);
    if (et == &PyUnicode_Type)    return _select_types(ctx, fid, stSTR);
    if (et == &PyBool_Type)       return _select_types(ctx, fid, stBOOL);
    if (et == &PyBaseObject_Type) return _select_types(ctx, fid, stOBJ);
  }
  if (value.is_ltype()) {
    auto lt = static_cast<LType>(value.get_attr("value").to_size_t());
    if (lt == LType::BOOL)     return _select_types(ctx, fid, stBOOL);
    if (lt == LType::INT)      return _select_types(ctx, fid, stINT);
    if (lt == LType::REAL)     return _select_types(ctx, fid, stFLOAT);
    if (lt == LType::STRING)   return _select_types(ctx, fid, stSTR);
    if (lt == LType::DATETIME) return _select_types(ctx, fid, stTIME);
    if (lt == LType::OBJECT)   return _select_types(ctx, fid, stOBJ);
  }
  if (value.is_stype()) {
    auto st = static_cast<SType>(value.get_attr("value").to_size_t());
    return _select_type(ctx, fid, st);
  }
  throw ValueError() << "Unknown type " << value << " used as a selector";
}



Workframe Head_Literal_Type::evaluate_j(
    const vecExpr&, EvalContext& ctx, bool allow_new) const
{
  return evaluate_f(ctx, 0, allow_new);
}


RowIndex Head_Literal_Type::evaluate_i(const vecExpr&, EvalContext&) const {
  throw TypeError() << "A type cannot be used as a row selector";
}


RiGb Head_Literal_Type::evaluate_iby(const vecExpr&, EvalContext&) const {
  throw TypeError() << "A type cannot be used as a row selector";
}




}}  // namespace dt::expr
