//------------------------------------------------------------------------------
// Copyright 2020-2023 H2O.ai
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
#include "documentation.h"
#include "expr/expr.h"            // OldExpr
#include "expr/fexpr.h"
#include "expr/fexpr_alias.h"
#include "expr/fexpr_column.h"
#include "expr/fexpr_dict.h"
#include "expr/fexpr_extend_remove.h"
#include "expr/fexpr_frame.h"
#include "expr/fexpr_list.h"
#include "expr/fexpr_literal.h"
#include "expr/fexpr_slice.h"
#include "expr/re/fexpr_match.h"
#include "expr/str/fexpr_len.h"
#include "python/obj.h"
#include "python/xargs.h"
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
  auto fexpr = reinterpret_cast<PyFExpr*>(src.to_borrowed_ref());
  return fexpr->get_expr();
}


ptrExpr as_fexpr(py::robj src) {
  if (src.is_fexpr())              return extract_fexpr(src);
  else if (src.is_dtexpr())        return std::make_shared<OldExpr>(src);
  else if (src.is_int())           return FExpr_Literal_Int::make(src);
  else if (src.is_string())        return FExpr_Literal_String::make(src);
  else if (src.is_float())         return FExpr_Literal_Float::make(src);
  else if (src.is_bool())          return FExpr_Literal_Bool::make(src);
  else if (src.is_slice())         return FExpr_Literal_Slice::make(src);
  else if (src.is_list_or_tuple()) return FExpr_List::make(src);
  else if (src.is_dict())          return FExpr_Dict::make(src);
  else if (src.is_anytype())       return FExpr_Literal_Type::make(src);
  else if (src.is_generator())     return FExpr_List::make(src);
  else if (src.is_none())          return FExpr_Literal_None::make();
  else if (src.is_frame())         return FExpr_Frame::from_datatable(src);
  else if (src.is_range())         return FExpr_Literal_Range::make(src);
  else if (src.is_pandas_frame() ||
           src.is_pandas_series()) return FExpr_Frame::from_pandas(src);
  else if (src.is_numpy_array() ||
           src.is_numpy_marray())  return FExpr_Frame::from_numpy(src);
  else if (src.is_numpy_float())   return FExpr_Literal_Float::make(src);
  else if (src.is_numpy_int())     return FExpr_Literal_Int::make(src);
  else if (src.is_numpy_bool())    return FExpr_Literal_Bool::make(src);
  else if (src.is_ellipsis())      return ptrExpr(new FExpr_Literal_SliceAll());
  else {
    throw TypeError() << "An object of type " << src.typeobj()
                      << " cannot be used in an FExpr";
  }
}



//------------------------------------------------------------------------------
// PyFExpr class
//------------------------------------------------------------------------------
using namespace py;

// static "constructor"
oobj PyFExpr::make(FExpr* expr) {
  xassert(FExpr_Type);
  oobj res = robj(FExpr_Type).call();
  auto fexpr = reinterpret_cast<PyFExpr*>(res.to_borrowed_ref());
  fexpr->expr_ = ptrExpr(expr);
  return res;
}

ptrExpr PyFExpr::get_expr() const {
  return expr_;
}



static PKArgs args__init__(1, 0, 0, false, false, {"e"}, "__init__", nullptr);

void PyFExpr::m__init__(const PKArgs& args) {
  auto arg = args[0].to_oobj();
  if (arg) {
    expr_ = as_fexpr(arg);
  }
}


void PyFExpr::m__dealloc__() {
  expr_ = nullptr;
}

oobj PyFExpr::m__repr__() const {
  // Normally we would never create an object with an empty `expr_`,
  // but if the user tries to instantiate it manually then the
  // `expr_` may end up as nullptr.
  if (!expr_) return ostring("FExpr<>");
  return ostring("FExpr<" + expr_->repr() + '>');
}

oobj PyFExpr::m__getitem__(py::robj item) {
  if (item.is_slice()) {
    auto slice = item.to_oslice();
    return PyFExpr::make(
                new dt::expr::FExpr_Slice(
                    expr_,
                    slice.start_obj(),
                    slice.stop_obj(),
                    slice.step_obj()
           ));
  }
  // TODO: we could also support single-item selectors
  throw TypeError() << "Selector inside FExpr[...] must be a slice";
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


oobj PyFExpr::nb__and__(robj lhs, robj rhs)      { return make_binexpr(dt::expr::Op::AND,      lhs, rhs); }
oobj PyFExpr::nb__xor__(robj lhs, robj rhs)      { return make_binexpr(dt::expr::Op::XOR,      lhs, rhs); }
oobj PyFExpr::nb__or__(robj lhs, robj rhs)       { return make_binexpr(dt::expr::Op::OR,       lhs, rhs); }
oobj PyFExpr::nb__lshift__(robj lhs, robj rhs)   { return make_binexpr(dt::expr::Op::LSHIFT,   lhs, rhs); }
oobj PyFExpr::nb__rshift__(robj lhs, robj rhs)   { return make_binexpr(dt::expr::Op::RSHIFT,   lhs, rhs); }

bool PyFExpr::nb__bool__() {
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

oobj PyFExpr::nb__invert__() {
  return make_unexpr(dt::expr::Op::UINVERT, this);
}

oobj PyFExpr::nb__neg__() {
  return make_unexpr(dt::expr::Op::UMINUS, this);
}

oobj PyFExpr::nb__pos__() {
  return make_unexpr(dt::expr::Op::UPLUS, this);
}




//----- Other methods ----------------------------------------------------------

oobj PyFExpr::extend(const XArgs& args) {
  auto arg = args[0].to_oobj();   
  return PyFExpr::make(new FExpr_Extend_Remove<true>(ptrExpr(expr_), as_fexpr(arg)));

}

DECLARE_METHOD(&PyFExpr::extend)
    ->name("extend")
    ->docs(dt::doc_FExpr_extend)
    ->n_positional_args(1)
    ->n_required_args(1)
    ->arg_names({"arg"});



oobj PyFExpr::remove(const XArgs& args) {
  auto arg = args[0].to_oobj();   
  return PyFExpr::make(new FExpr_Extend_Remove<false>(ptrExpr(expr_), as_fexpr(arg)));

}

DECLARE_METHOD(&PyFExpr::remove)
    ->name("remove")
    ->docs(dt::doc_FExpr_remove)
    ->n_positional_args(1)
    ->n_required_args(1)
    ->arg_names({"arg"});



oobj PyFExpr::len() {
  auto w = DeprecationWarning();
  w << "Method Expr.len() is deprecated since 0.11.0, "
       "and will be removed in version 1.1.\n"
       "Please use function dt.str.len() instead";
  w.emit_warning();
  return PyFExpr::make(new FExpr_Str_Len(ptrExpr(expr_)));
}



oobj PyFExpr::re_match(const XArgs& args) {
  auto arg_pattern = args[0].to_oobj_or_none();
  auto w = DeprecationWarning();
  w << "Method Expr.re_match() is deprecated since 0.11.0, "
       "and will be removed in version 1.1.\n"
       "Please use function dt.re.match() instead";
  w.emit_warning();
  return PyFExpr::make(new FExpr_Re_Match(ptrExpr(expr_), arg_pattern, py::False()));
}

DECLARE_METHOD(&PyFExpr::re_match)
    ->name("re_match")
    ->n_positional_or_keyword_args(1)
    ->arg_names({"pattern"});



//------------------------------------------------------------------------------
// Miscellaneous
//------------------------------------------------------------------------------

oobj PyFExpr::alias(const XArgs& args) {
  strvec names_vec;
  size_t argi = 0;

  for (auto arg : args.varargs()) {
    if (arg.is_string()) {
      names_vec.push_back(arg.to_string());
    } else if (arg.is_list_or_tuple()) {
      py::oiter names_iter = arg.to_oiter();
      names_vec.reserve(names_iter.size());
      size_t namei = 0;

      for (auto name : names_iter) {
        if (name.is_string()) {
          names_vec.emplace_back(name.to_string());
        } else {
          throw TypeError()
            << "`datatable.FExpr.alias()` expects all elements of lists/tuples "
            << "of names to be strings, instead for name `" << argi << "` "
            << "element `" << namei << "` is "
            << name.typeobj();
        }
        namei++;
      }

    } else {
      throw TypeError()
        << "`datatable.FExpr.alias()` expects all names to be strings, or "
        << "lists/tuples of strings, instead name `" << argi << "` is "
        << arg.typeobj();
    }
    argi++;
  }

  return PyFExpr::make(new FExpr_Alias(ptrExpr(expr_), std::move(names_vec)));

}


DECLARE_METHOD(&PyFExpr::alias)
   ->name("alias")
   ->docs(dt::doc_FExpr_alias)
   ->allow_varargs();


oobj PyFExpr::as_type(const XArgs& args) {
  auto as_typeFn = oobj::import("datatable", "as_type");
  oobj new_type = args[0].to_oobj();
  return as_typeFn.call({this, new_type});
}

DECLARE_METHOD(&PyFExpr::as_type)
    ->name("as_type")
    ->docs(dt::doc_FExpr_as_type)
    ->arg_names({"new_type"})
    ->n_positional_args(1)
    ->n_required_args(1);


oobj PyFExpr::categories(const XArgs&) {
  auto categoriesFn = oobj::import("datatable", "categories");
  return categoriesFn.call({this});
}

DECLARE_METHOD(&PyFExpr::categories)
    ->name("categories")
    ->docs(dt::doc_FExpr_categories);


oobj PyFExpr::codes(const XArgs&) {
  auto codesFn = oobj::import("datatable", "codes");
  return codesFn.call({this});
}

DECLARE_METHOD(&PyFExpr::codes)
    ->name("codes")
    ->docs(dt::doc_FExpr_codes);


oobj PyFExpr::count(const XArgs&) {
  auto countFn = oobj::import("datatable", "count");
  return countFn.call({this});
}

DECLARE_METHOD(&PyFExpr::count)
    ->name("count")
    ->docs(dt::doc_FExpr_count);


oobj PyFExpr::countna(const XArgs&) {
  auto countnaFn = oobj::import("datatable", "countna");
  return countnaFn.call({this});
}

DECLARE_METHOD(&PyFExpr::countna)
    ->name("countna")
    ->docs(dt::doc_FExpr_countna);

oobj PyFExpr::cummax(const XArgs& args) {
  auto cummaxFn = oobj::import("datatable", "cummax");
  oobj reverse = args[0]? args[0].to_oobj() : py::obool(false);
  return cummaxFn.call({this, reverse});
}

DECLARE_METHOD(&PyFExpr::cummax)
    ->name("cummax")
    ->docs(dt::doc_FExpr_cummax)
    ->arg_names({"reverse"})
    ->n_positional_or_keyword_args(1)
    ->n_required_args(0);


oobj PyFExpr::cummin(const XArgs& args) {
  auto cumminFn = oobj::import("datatable", "cummin");
  oobj reverse = args[0]? args[0].to_oobj() : py::obool(false);
  return cumminFn.call({this, reverse});
}

DECLARE_METHOD(&PyFExpr::cummin)
    ->name("cummin")
    ->docs(dt::doc_FExpr_cummin)
    ->arg_names({"reverse"})
    ->n_positional_or_keyword_args(1)
    ->n_required_args(0);


oobj PyFExpr::cumprod(const XArgs& args) {
  auto cumprodFn = oobj::import("datatable", "cumprod");
  oobj reverse = args[0]? args[0].to_oobj() : py::obool(false);
  return cumprodFn.call({this, reverse});
}

DECLARE_METHOD(&PyFExpr::cumprod)
    ->name("cumprod")
    ->docs(dt::doc_FExpr_cumprod)
    ->arg_names({"reverse"})
    ->n_positional_or_keyword_args(1)
    ->n_required_args(0);


oobj PyFExpr::cumsum(const XArgs& args) {
  auto cumsumFn = oobj::import("datatable", "cumsum");
  oobj reverse = args[0]? args[0].to_oobj() : py::obool(false);
  return cumsumFn.call({this, reverse});
}

DECLARE_METHOD(&PyFExpr::cumsum)
    ->name("cumsum")
    ->docs(dt::doc_FExpr_cumsum)
    ->arg_names({"reverse"})
    ->n_positional_or_keyword_args(1)
    ->n_required_args(0);



oobj PyFExpr::fillna(const XArgs& args) {
  auto fillnaFn = oobj::import("datatable", "fillna");
  oobj value = args[0].to_oobj_or_none();
  oobj reverse = args[1].to_oobj_or_none();

  py::odict kws;
  kws.set(py::ostring("value"), value);
  kws.set(py::ostring("reverse"), reverse);

  return fillnaFn.call({this}, kws);
}

DECLARE_METHOD(&PyFExpr::fillna)
    ->name("fillna")
    ->docs(dt::doc_FExpr_fillna)
    ->arg_names({"value", "reverse"})
    ->n_keyword_args(2)
    ->n_required_args(0);


oobj PyFExpr::first(const XArgs&) {
  auto firstFn = oobj::import("datatable", "first");
  return firstFn.call({this});
}

DECLARE_METHOD(&PyFExpr::first)
    ->name("first")
    ->docs(dt::doc_FExpr_first);


oobj PyFExpr::isna(const XArgs&) {
  auto isnaFn = oobj::import("datatable", "math", "isna");
  return isnaFn.call({this});
}

DECLARE_METHOD(&PyFExpr::isna)
    ->name("isna")
    ->docs(dt::doc_FExpr_isna);


oobj PyFExpr::last(const XArgs&) {
  auto lastFn = oobj::import("datatable", "last");
  return lastFn.call({this});
}

DECLARE_METHOD(&PyFExpr::last)
    ->name("last")
    ->docs(dt::doc_FExpr_last);

oobj PyFExpr::max(const XArgs&) {
  auto maxFn = oobj::import("datatable", "max");
  return maxFn.call({this});
}

DECLARE_METHOD(&PyFExpr::max)
    ->name("max")
    ->docs(dt::doc_FExpr_max);



oobj PyFExpr::mean(const XArgs&) {
  auto meanFn = oobj::import("datatable", "mean");
  return meanFn.call({this});
}

DECLARE_METHOD(&PyFExpr::mean)
    ->name("mean")
    ->docs(dt::doc_FExpr_mean);



oobj PyFExpr::median(const XArgs&) {
  auto medianFn = oobj::import("datatable", "median");
  return medianFn.call({this});
}

DECLARE_METHOD(&PyFExpr::median)
    ->name("median")
    ->docs(dt::doc_FExpr_median);



oobj PyFExpr::min(const XArgs&) {
  auto minFn = oobj::import("datatable", "min");
  return minFn.call({this});
}

DECLARE_METHOD(&PyFExpr::min)
    ->name("min")
    ->docs(dt::doc_FExpr_min);


oobj PyFExpr::nunique(const XArgs&) {
  auto nuniqueFn = oobj::import("datatable", "nunique");
  return nuniqueFn.call({this});
}

DECLARE_METHOD(&PyFExpr::nunique)
    ->name("nunique")
    ->docs(dt::doc_FExpr_nunique);

oobj PyFExpr::prod(const XArgs&) {
  auto prodFn = oobj::import("datatable", "prod");
  return prodFn.call({this});
}

DECLARE_METHOD(&PyFExpr::prod)
    ->name("prod")
    ->docs(dt::doc_FExpr_prod);



oobj PyFExpr::rowall(const XArgs&) {
  auto rowallFn = oobj::import("datatable", "rowall");
  return rowallFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowall)
    ->name("rowall")
    ->docs(dt::doc_FExpr_rowall);



oobj PyFExpr::rowany(const XArgs&) {
  auto rowanyFn = oobj::import("datatable", "rowany");
  return rowanyFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowany)
    ->name("rowany")
    ->docs(dt::doc_FExpr_rowany);

oobj PyFExpr::rowargmax(const XArgs&) {
  auto rowargmaxFn = oobj::import("datatable", "rowargmax");
  return rowargmaxFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowargmax)
    ->name("rowargmax")
    ->docs(dt::doc_FExpr_rowargmax);

oobj PyFExpr::rowargmin(const XArgs&) {
  auto rowargminFn = oobj::import("datatable", "rowargmin");
  return rowargminFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowargmin)
    ->name("rowargmin")
    ->docs(dt::doc_FExpr_rowargmin);


oobj PyFExpr::rowcount(const XArgs&) {
  auto rowcountFn = oobj::import("datatable", "rowcount");
  return rowcountFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowcount)
    ->name("rowcount")
    ->docs(dt::doc_FExpr_rowcount);



oobj PyFExpr::rowfirst(const XArgs&) {
  auto rowfirstFn = oobj::import("datatable", "rowfirst");
  return rowfirstFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowfirst)
    ->name("rowfirst")
    ->docs(dt::doc_FExpr_rowfirst);



oobj PyFExpr::rowlast(const XArgs&) {
  auto rowlastFn = oobj::import("datatable", "rowlast");
  return rowlastFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowlast)
    ->name("rowlast")
    ->docs(dt::doc_FExpr_rowlast);




oobj PyFExpr::rowmax(const XArgs&) {
  auto rowmaxFn = oobj::import("datatable", "rowmax");
  return rowmaxFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowmax)
    ->name("rowmax")
    ->docs(dt::doc_FExpr_rowmax);



oobj PyFExpr::rowmean(const XArgs&) {
  auto rowmeanFn = oobj::import("datatable", "rowmean");
  return rowmeanFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowmean)
    ->name("rowmean")
    ->docs(dt::doc_FExpr_rowmean);



oobj PyFExpr::rowmin(const XArgs&) {
  auto rowminFn = oobj::import("datatable", "rowmin");
  return rowminFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowmin)
    ->name("rowmin")
    ->docs(dt::doc_FExpr_rowmin);


oobj PyFExpr::rowsd(const XArgs&) {
  auto rowsdFn = oobj::import("datatable", "rowsd");
  return rowsdFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowsd)
    ->name("rowsd")
    ->docs(dt::doc_FExpr_rowsd);



oobj PyFExpr::rowsum(const XArgs&) {
  auto rowsumFn = oobj::import("datatable", "rowsum");
  return rowsumFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowsum)
    ->name("rowsum")
    ->docs(dt::doc_FExpr_rowsum);


oobj PyFExpr::sd(const XArgs&) {
  auto sdFn = oobj::import("datatable", "sd");
  return sdFn.call({this});
}

DECLARE_METHOD(&PyFExpr::sd)
    ->name("sd")
    ->docs(dt::doc_FExpr_sd);


oobj PyFExpr::shift(const XArgs& args) {
  auto shiftFn = oobj::import("datatable", "shift");
  oobj n = args[0]? args[0].to_oobj() : py::oint(1);
  return shiftFn.call({this, n});
}

DECLARE_METHOD(&PyFExpr::shift)
    ->name("shift")
    ->docs(dt::doc_FExpr_shift)
    ->arg_names({"n"})
    ->n_positional_or_keyword_args(1);

oobj PyFExpr::sum(const XArgs&) {
  auto sumFn = oobj::import("datatable", "sum");
  return sumFn.call({this});
}

DECLARE_METHOD(&PyFExpr::sum)
    ->name("sum")
    ->docs(dt::doc_FExpr_sum);



//------------------------------------------------------------------------------
// Class decoration
//------------------------------------------------------------------------------

void PyFExpr::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.FExpr");
  xt.set_class_doc(dt::doc_FExpr);
  xt.set_subclassable(false);

  xt.add(CONSTRUCTOR(&PyFExpr::m__init__, args__init__));
  xt.add(DESTRUCTOR(&PyFExpr::m__dealloc__));
  xt.add(METHOD0(&PyFExpr::len, "len"));

  xt.add(METHOD__REPR__(&PyFExpr::m__repr__));
  xt.add(METHOD__ADD__(&PyFExpr::nb__add__));
  xt.add(METHOD__SUB__(&PyFExpr::nb__sub__));
  xt.add(METHOD__MUL__(&PyFExpr::nb__mul__));
  xt.add(METHOD__TRUEDIV__(&PyFExpr::nb__truediv__));
  xt.add(METHOD__FLOORDIV__(&PyFExpr::nb__floordiv__));
  xt.add(METHOD__MOD__(&PyFExpr::nb__mod__));
  xt.add(METHOD__AND__(&PyFExpr::nb__and__));
  xt.add(METHOD__XOR__(&PyFExpr::nb__xor__));
  xt.add(METHOD__OR__(&PyFExpr::nb__or__));
  xt.add(METHOD__LSHIFT__(&PyFExpr::nb__lshift__));
  xt.add(METHOD__RSHIFT__(&PyFExpr::nb__rshift__));
  xt.add(METHOD__POW__(&PyFExpr::nb__pow__));
  xt.add(METHOD__BOOL__(&PyFExpr::nb__bool__));
  xt.add(METHOD__INVERT__(&PyFExpr::nb__invert__));
  xt.add(METHOD__NEG__(&PyFExpr::nb__neg__));
  xt.add(METHOD__POS__(&PyFExpr::nb__pos__));
  xt.add(METHOD__CMP__(&PyFExpr::m__compare__));
  xt.add(METHOD__GETITEM__(&PyFExpr::m__getitem__));

  FExpr_Type = xt.get_type_object();

  INIT_METHODS_FOR_CLASS(PyFExpr);
}




}}  // namespace dt::expr
