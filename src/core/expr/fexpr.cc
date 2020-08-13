//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
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
#include "expr/fexpr.h"
#include "expr/fexpr_literal.h"
#include "expr/expr.h"            // OldExpr
#include "python/obj.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// dt::expr::FExpr class
//------------------------------------------------------------------------------

ptrExpr FExpr::unnegate_column() const {
  return nullptr;
}


bool FExpr::evaluate_bool() const {
  throw RuntimeError();  // LCOV_EXCL_LINE
}                        // LCOV_EXCL_LINE


int64_t FExpr::evaluate_int() const {
  throw RuntimeError();  // LCOV_EXCL_LINE
}                        // LCOV_EXCL_LINE


py::oobj FExpr::evaluate_pystr() const {
  throw RuntimeError();  // LCOV_EXCL_LINE
}                        // LCOV_EXCL_LINE


void FExpr::prepare_by(EvalContext&, Workframe&, std::vector<SortFlag>&) const {
  throw RuntimeError();  // LCOV_EXCL_LINE
}                        // LCOV_EXCL_LINE




//------------------------------------------------------------------------------
// as_fexpr()
//------------------------------------------------------------------------------

static ptrExpr extract_fexpr(py::robj src) {
  xassert(src.is_fexpr());
  auto fexpr = reinterpret_cast<py::FExpr*>(src.to_borrowed_ref());
  return fexpr->get_expr();
}


ptrExpr as_fexpr(py::robj src) {
  if (src.is_fexpr())              return extract_fexpr(src);
  else if (src.is_dtexpr())        ;
  else if (src.is_int())           return FExpr_Literal_Int::make(src);
  else if (src.is_string())        return FExpr_Literal_String::make(src);
  else if (src.is_float())         return FExpr_Literal_Float::make(src);
  else if (src.is_bool())          return FExpr_Literal_Bool::make(src);
  else if (src.is_slice())         return FExpr_Literal_Slice::make(src);
  else if (src.is_list_or_tuple()) ;
  else if (src.is_dict())          ;
  else if (src.is_anytype())       ;
  else if (src.is_generator())     ;
  else if (src.is_none())          return FExpr_Literal_None::make();
  else if (src.is_frame())         ;
  else if (src.is_range())         return FExpr_Literal_Range::make(src);
  else if (src.is_pandas_frame() ||
           src.is_pandas_series()) ;
  else if (src.is_numpy_array() ||
           src.is_numpy_marray())  ;
  else if (src.is_ellipsis())      return ptrExpr(new FExpr_Literal_SliceAll());
  else {
    throw TypeError() << "An object of type " << src.typeobj()
                      << " cannot be used in an FExpr";
  }
  // TODO: remove this
  return std::make_shared<OldExpr>(src);
}



}}
//------------------------------------------------------------------------------
// Python FExpr class
//------------------------------------------------------------------------------
namespace py {

// static "constructor"
oobj FExpr::make(dt::expr::FExpr* expr) {
  oobj res = robj(reinterpret_cast<PyObject*>(FExpr_Type)).call();
  auto fexpr = reinterpret_cast<FExpr*>(res.to_borrowed_ref());
  fexpr->expr_ = dt::expr::ptrExpr(expr);
  return res;
}

std::shared_ptr<dt::expr::FExpr> FExpr::get_expr() const {
  return expr_;
}



static PKArgs args__init__(0, 0, 0, false, false, {}, "__init__", nullptr);

void FExpr::m__init__(const PKArgs&) {}

void FExpr::m__dealloc__() {
  expr_ = nullptr;
}

oobj FExpr::m__repr__() const {
  // Normally we would never create an object with an empty `expr_`,
  // but if the user tries to instantiate it manually then the
  // `expr_` will end up as nullptr.
  if (!expr_) return ostring("FExpr<>");
  return ostring("FExpr<" + expr_->repr() + '>');
}



//----- Basic arithmetics ------------------------------------------------------

static oobj make_unexpr(dt::expr::Op op, const PyObject* self) {
  return robj(Expr_Type).call({
                  oint(static_cast<int>(op)),
                  otuple{oobj(self)}});
}

static oobj make_binexpr(dt::expr::Op op, robj lhs, robj rhs) {
  return robj(Expr_Type).call({
                  oint(static_cast<int>(op)),
                  otuple{lhs, rhs}});
}


oobj FExpr::nb__add__(robj lhs, robj rhs)      { return make_binexpr(dt::expr::Op::PLUS,     lhs, rhs); }
oobj FExpr::nb__sub__(robj lhs, robj rhs)      { return make_binexpr(dt::expr::Op::MINUS,    lhs, rhs); }
oobj FExpr::nb__mul__(robj lhs, robj rhs)      { return make_binexpr(dt::expr::Op::MULTIPLY, lhs, rhs); }
oobj FExpr::nb__truediv__(robj lhs, robj rhs)  { return make_binexpr(dt::expr::Op::DIVIDE,   lhs, rhs); }
oobj FExpr::nb__floordiv__(robj lhs, robj rhs) { return make_binexpr(dt::expr::Op::INTDIV,   lhs, rhs); }
oobj FExpr::nb__mod__(robj lhs, robj rhs)      { return make_binexpr(dt::expr::Op::MODULO,   lhs, rhs); }
oobj FExpr::nb__and__(robj lhs, robj rhs)      { return make_binexpr(dt::expr::Op::AND,      lhs, rhs); }
oobj FExpr::nb__xor__(robj lhs, robj rhs)      { return make_binexpr(dt::expr::Op::XOR,      lhs, rhs); }
oobj FExpr::nb__or__(robj lhs, robj rhs)       { return make_binexpr(dt::expr::Op::OR,       lhs, rhs); }
oobj FExpr::nb__lshift__(robj lhs, robj rhs)   { return make_binexpr(dt::expr::Op::LSHIFT,   lhs, rhs); }
oobj FExpr::nb__rshift__(robj lhs, robj rhs)   { return make_binexpr(dt::expr::Op::RSHIFT,   lhs, rhs); }

oobj FExpr::nb__pow__(robj lhs, robj rhs, robj zhs) {
  if (zhs && !zhs.is_none()) {
    throw NotImplError() << "2-argument form of pow() is not supported";
  }
  return make_binexpr(dt::expr::Op::POWEROP, lhs, rhs);
}

bool FExpr::nb__bool__() {
  throw TypeError() <<
      "Expression " << expr_->repr() << " cannot be cast to bool.\n\n"
      "You may be seeing this error because either:\n"
      "  * you tried to use chained inequality such as\n"
      "        0 < f.A < 100\n"
      "    If so please rewrite it as\n"
      "        (0 < f.A) & (f.A < 100)\n\n"
      "  * you used keywords and/or, for example\n"
      "        f.A < 0 or f.B >= 1\n"
      "    If so then replace keywords with operators `&` or `|`:\n"
      "        (f.A < 0) | (f.B >= 1)\n"
      "    Be mindful that `&` / `|` have higher precedence than `and`\n"
      "    or `or`, so make sure to use parentheses appropriately.\n\n"
      "  * you used expression in the `if` statement, for example:\n"
      "        f.A if f.A > 0 else -f.A\n"
      "    You may write this as a ternary operator instead:\n"
      "        (f.A > 0) & f.A | -f.A\n\n"
      "  * you explicitly cast the expression into `bool`:\n"
      "        bool(f.B)\n"
      "    this can be replaced with an explicit comparison operator:\n"
      "        f.B != 0\n";
}

oobj FExpr::nb__invert__() {
  return make_unexpr(dt::expr::Op::UINVERT, this);
}

oobj FExpr::nb__neg__() {
  return make_unexpr(dt::expr::Op::UMINUS, this);
}

oobj FExpr::nb__pos__() {
  return make_unexpr(dt::expr::Op::UPLUS, this);
}


oobj FExpr::m__compare__(robj x, robj y, int op) {
  switch (op) {
    case Py_EQ: return make_binexpr(dt::expr::Op::EQ, x, y);
    case Py_NE: return make_binexpr(dt::expr::Op::NE, x, y);
    case Py_LT: return make_binexpr(dt::expr::Op::LT, x, y);
    case Py_LE: return make_binexpr(dt::expr::Op::LE, x, y);
    case Py_GT: return make_binexpr(dt::expr::Op::GT, x, y);
    case Py_GE: return make_binexpr(dt::expr::Op::GE, x, y);
    default:
      throw RuntimeError() << "Unknown op " << op << " in __compare__";  // LCOV_EXCL_LINE
  }
}


//----- Other methods ----------------------------------------------------------

// TODO
static const char* doc_extend =
R"(extend(self, arg)
--
)";

static PKArgs args_extend(1, 0, 0, false, false, {"arg"}, "extend", doc_extend);

oobj FExpr::extend(const PKArgs& args) {
  auto arg = args[0].to<oobj>(py::None());
  return make_binexpr(dt::expr::Op::SETPLUS, robj(this), arg);
}


// TODO
static const char* doc_remove =
R"(remove(self, arg)
--
)";

static PKArgs args_remove(1, 0, 0, false, false, {"arg"}, "remove", doc_remove);

oobj FExpr::remove(const PKArgs& args) {
  auto arg = args[0].to_oobj();
  return make_binexpr(dt::expr::Op::SETMINUS, robj(this), arg);
}


// DEPRECATED
oobj FExpr::len() {
  return make_unexpr(dt::expr::Op::LEN, this);
}


static PKArgs args_re_match(
    0, 2, 0, false, false, {"pattern", "flags"}, "re_match", nullptr);

oobj FExpr::re_match(const PKArgs& args) {
  auto arg0 = args[0].to<oobj>(py::None());
  auto arg1 = args[1].to<oobj>(py::None());
  return robj(Expr_Type).call({
                  oint(static_cast<int>(dt::expr::Op::RE_MATCH)),
                  otuple{robj(this)},
                  otuple{arg0, arg1}});
}



// TODO
static const char* doc_fexpr =
R"()";

void FExpr::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.FExpr");
  xt.set_class_doc(doc_fexpr);
  xt.set_subclassable(false);

  xt.add(CONSTRUCTOR(&FExpr::m__init__, args__init__));
  xt.add(DESTRUCTOR(&FExpr::m__dealloc__));
  xt.add(METHOD(&FExpr::extend, args_extend));
  xt.add(METHOD(&FExpr::remove, args_remove));
  xt.add(METHOD0(&FExpr::len, "len"));
  xt.add(METHOD(&FExpr::re_match, args_re_match));

  xt.add(METHOD__REPR__(&FExpr::m__repr__));
  xt.add(METHOD__ADD__(&FExpr::nb__add__));
  xt.add(METHOD__SUB__(&FExpr::nb__sub__));
  xt.add(METHOD__MUL__(&FExpr::nb__mul__));
  xt.add(METHOD__TRUEDIV__(&FExpr::nb__truediv__));
  xt.add(METHOD__FLOORDIV__(&FExpr::nb__floordiv__));
  xt.add(METHOD__MOD__(&FExpr::nb__mod__));
  xt.add(METHOD__AND__(&FExpr::nb__and__));
  xt.add(METHOD__XOR__(&FExpr::nb__xor__));
  xt.add(METHOD__OR__(&FExpr::nb__or__));
  xt.add(METHOD__LSHIFT__(&FExpr::nb__lshift__));
  xt.add(METHOD__RSHIFT__(&FExpr::nb__rshift__));
  xt.add(METHOD__POW__(&FExpr::nb__pow__));
  xt.add(METHOD__BOOL__(&FExpr::nb__bool__));
  xt.add(METHOD__INVERT__(&FExpr::nb__invert__));
  xt.add(METHOD__NEG__(&FExpr::nb__neg__));
  xt.add(METHOD__POS__(&FExpr::nb__pos__));
  xt.add(METHOD__CMP__(&FExpr::m__compare__));

  FExpr_Type = &type;
}




}  // namespace py
