//------------------------------------------------------------------------------
// Copyright 2019-2021 H2O.ai
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

static stypevec stVOID  = {SType::VOID};
static stypevec stBOOL  = {SType::BOOL};
static stypevec stINT   = {SType::INT8, SType::INT16, SType::INT32, SType::INT64};
static stypevec stFLOAT = {SType::FLOAT32, SType::FLOAT64};
static stypevec stSTR   = {SType::STR32, SType::STR64};
static stypevec stDATE  = {SType::DATE32};
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
  if (value_.is_pytype()) {
    auto et = reinterpret_cast<PyTypeObject*>(value_.to_borrowed_ref());
    if (et == &PyLong_Type)       return _select_types(ctx, fid, stINT);
    if (et == &PyFloat_Type)      return _select_types(ctx, fid, stFLOAT);
    if (et == &PyUnicode_Type)    return _select_types(ctx, fid, stSTR);
    if (et == &PyBool_Type)       return _select_types(ctx, fid, stBOOL);
    if (et == &PyBaseObject_Type) return _select_types(ctx, fid, stOBJ);
    if (et == py::odate::type()) {
      return _select_types(ctx, fid, stDATE);
    }
  }
  if (value_.is_ltype()) {
    auto lt = static_cast<LType>(value_.get_attr("value").to_size_t());
    if (lt == LType::MU)       return _select_types(ctx, fid, stVOID);
    if (lt == LType::BOOL)     return _select_types(ctx, fid, stBOOL);
    if (lt == LType::INT)      return _select_types(ctx, fid, stINT);
    if (lt == LType::REAL)     return _select_types(ctx, fid, stFLOAT);
    if (lt == LType::STRING)   return _select_types(ctx, fid, stSTR);
    if (lt == LType::DATETIME) return _select_types(ctx, fid, stDATE);
    if (lt == LType::OBJECT)   return _select_types(ctx, fid, stOBJ);
  }
  if (value_.is_type()) {
    auto st = value_.to_type().stype();
    return _select_type(ctx, fid, st);
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


//------------------------------------------------------------------------------

class TypeMatcher {
  public:
    virtual ~TypeMatcher() {}
    virtual dt::Type convert(const dt::Type&) const = 0;
};

class PyLong_TypeMatcher : public TypeMatcher {
  public:
    dt::Type convert(const dt::Type& inType) const override {
      return (inType.is_integer())? inType : dt::Type::int32();
    }
};

class PyFloat_TypeMatcher : public TypeMatcher {
  public:
    dt::Type convert(const dt::Type& inType) const override {
      return (inType.is_float())? inType : dt::Type::float64();
    }
};

class PyUnicode_TypeMatcher : public TypeMatcher {
  public:
    dt::Type convert(const dt::Type& inType) const override {
      return (inType.is_string())? inType : dt::Type::str32();
    }
};

class PyBool_TypeMatcher : public TypeMatcher {
  public:
    dt::Type convert(const dt::Type& inType) const override {
      return (inType.is_boolean())? inType : dt::Type::bool8();
    }
};

class PyObject_TypeMatcher : public TypeMatcher {
  public:
    dt::Type convert(const dt::Type& inType) const override {
      return (inType.is_object())? inType : dt::Type::obj64();
    }
};

class Type_TypeMatcher : public TypeMatcher {
  private:
    dt::Type targetType_;
  public:
    Type_TypeMatcher(dt::Type type) : targetType_(type) {}
    dt::Type convert(const dt::Type&) const override {
      return targetType_;
    }
};

class SType_TypeMatcher : public TypeMatcher {
  private:
    SType targetSType_;
    size_t : 56;
  public:
    SType_TypeMatcher(SType stype) : targetSType_(stype) {}
    dt::Type convert(const dt::Type& inType) const override {
      return inType && inType.stype() == targetSType_
        ? inType : dt::Type::from_stype(targetSType_);
    }
};

class LType_TypeMatcher : public TypeMatcher {
  private:
    LType targetLType_;
    SType targetSType_;
    size_t : 48;
  public:
    LType_TypeMatcher(LType ltype)
      : targetLType_(ltype),
        targetSType_(ltype == LType::BOOL? SType::BOOL :
                     ltype == LType::INT?  SType::INT32 :
                     ltype == LType::REAL? SType::FLOAT64 :
                     ltype == LType::STRING? SType::STR32 :
                     ltype == LType::OBJECT? SType::OBJ :
                     SType::INVALID)
    {}
    dt::Type convert(const dt::Type& inType) const override {
      return inType && stype_to_ltype(inType.stype()) == targetLType_
        ? inType : dt::Type::from_stype(targetSType_);
    }
};

using TMptr = std::unique_ptr<TypeMatcher>;
static TMptr _resolve_type(py::robj value_) {
  if (value_.is_pytype()) {
    auto et = reinterpret_cast<PyTypeObject*>(value_.to_borrowed_ref());
    if (et == &PyLong_Type)       return TMptr(new PyLong_TypeMatcher());
    if (et == &PyFloat_Type)      return TMptr(new PyFloat_TypeMatcher());
    if (et == &PyUnicode_Type)    return TMptr(new PyUnicode_TypeMatcher());
    if (et == &PyBool_Type)       return TMptr(new PyBool_TypeMatcher());
    if (et == &PyBaseObject_Type) return TMptr(new PyObject_TypeMatcher());
  }
  else if (value_.is_type()) {
    auto type = value_.to_type();
    return TMptr(new Type_TypeMatcher(type));
  }
  else if (value_.is_ltype()) {
    auto lt = value_.get_attr("value").to_size_t();
    if (lt < LTYPES_COUNT) {
      return TMptr(new LType_TypeMatcher(static_cast<LType>(lt)));
    }
  }
  else if (value_.is_stype()) {
    auto st = value_.get_attr("value").to_size_t();
    if (st < STYPES_COUNT) {
      return TMptr(new SType_TypeMatcher(static_cast<SType>(st)));
    }
  }
  throw ValueError() << "Unknown type " << value_
                     << " used in the replacement expression";
}


Workframe FExpr_Literal_Type::evaluate_r(
      EvalContext& ctx, const sztvec& indices) const
{
  if (ctx.get_rowindex(0)) {
    throw ValueError()
        << "Partial reassignment of Column's type is not possible";
  }
  auto typeMatcher = _resolve_type(value_);
  xassert(typeMatcher);

  auto dt0 = ctx.get_datatable(0);
  Workframe res(ctx);
  for (size_t i : indices) {
    Column newcol;
    if (i < dt0->ncols()) {
      newcol = dt0->get_column(i);  // copy
      newcol.cast_inplace( typeMatcher->convert(newcol.type()) );
    }
    else {
      newcol = Column::new_na_column(dt0->nrows(),
                    typeMatcher->convert(dt::Type()));
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
  if (value_.is_pytype()) {
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
