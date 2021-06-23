//------------------------------------------------------------------------------
// Copyright 2020-2021 H2O.ai
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
#include <iostream>
#include "expr/expr.h"            // OldExpr
#include "expr/fexpr.h"
#include "expr/fexpr_column.h"
#include "expr/fexpr_dict.h"
#include "expr/fexpr_frame.h"
#include "expr/fexpr_list.h"
#include "expr/fexpr_literal.h"
#include "expr/fexpr_slice.h"
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
  // Normally we would never create an object with an empty `expr_`,
  // but if the user tries to instantiate it manually then the
  // `expr_` may end up as nullptr.
  return PyFExpr::make(
              new dt::expr::FExpr_Slice(expr_, item));
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

static const char* doc_extend =
R"(extend(self, arg)
--

Append ``FExpr`` `arg` to the current FExpr.

Each ``FExpr`` represents a collection of columns, or a columnset. This
method takes two such columnsets and combines them into a single one,
similar to :func:`cbind() <datatable.cbind>`.


Parameters
----------
arg: FExpr
    The expression to append.

return: FExpr
    New FExpr which is a combination of the current FExpr and `arg`.


See also
--------
- :meth:`remove() <dt.FExpr.remove>` -- remove columns from a columnset.
)";

static PKArgs args_extend(1, 0, 0, false, false, {"arg"}, "extend", doc_extend);

oobj PyFExpr::extend(const PKArgs& args) {
  auto arg = args[0].to<oobj>(py::None());
  return make_binexpr(dt::expr::Op::SETPLUS, robj(this), arg);
}


static const char* doc_remove =
R"(remove(self, arg)
--

Remove columns `arg` from the current FExpr.

Each ``FExpr`` represents a collection of columns, or a columnset. Some
of those columns are computed while others are specified "by reference",
for example ``f.A``, ``f[:3]`` or ``f[int]``. This method allows you to
remove by-reference columns from an existing FExpr.


Parameters
----------
arg: FExpr
    The columns to remove. These must be "columns-by-reference", i.e.
    they cannot be computed columns.

return: FExpr
    New FExpr which is a obtained from the current FExpr by removing
    the columns in `arg`.


See also
--------
- :meth:`extend() <dt.FExpr.extend>` -- append a columnset.
)";

static PKArgs args_remove(1, 0, 0, false, false, {"arg"}, "remove", doc_remove);

oobj PyFExpr::remove(const PKArgs& args) {
  auto arg = args[0].to_oobj();
  return make_binexpr(dt::expr::Op::SETMINUS, robj(this), arg);
}


// DEPRECATED
oobj PyFExpr::len() {
  return make_unexpr(dt::expr::Op::LEN, this);
}


static const char* doc_re_match =
R"(re_match(self, pattern, flags=None)
--

.. x-version-deprecated:: 0.11

Test whether values in a string column match a regular expression.

Since version 1.0 this function will be available in the ``re.``
submodule.

Parameters
----------
pattern: str
    The regular expression that will be tested against each value
    in the current column.

flags: int
    [unused]

return: FExpr
    Return an expression that produces boolean column that tells
    whether the value in each row of the current column matches
    the `pattern` or not.
)";

static PKArgs args_re_match(
    0, 2, 0, false, false, {"pattern", "flags"}, "re_match", doc_re_match);

oobj PyFExpr::re_match(const PKArgs& args) {
  auto arg0 = args[0].to<oobj>(py::None());
  auto arg1 = args[1].to<oobj>(py::None());
  return robj(Expr_Type).call({
                  oint(static_cast<int>(dt::expr::Op::RE_MATCH)),
                  otuple{robj(this)},
                  otuple{arg0, arg1}});
}




//------------------------------------------------------------------------------
// Miscellaneous
//------------------------------------------------------------------------------

static const char* doc_sum =
R"(sum()
--

Equivalent to :func:`dt.sum(self)`.
)";

oobj PyFExpr::sum(const XArgs&) {
  auto sumFn = oobj::import("datatable", "sum");
  return sumFn.call({this});
}

DECLARE_METHOD(&PyFExpr::sum)
    ->name("sum")
    ->docs(doc_sum);

static const char* doc_max =
R"(max()
--

Equivalent to :func:`dt.max(self)`.
)";

oobj PyFExpr::max(const XArgs&) {
  auto maxFn = oobj::import("datatable", "max");
  return maxFn.call({this});
}

DECLARE_METHOD(&PyFExpr::max)
    ->name("max")
    ->docs(doc_max);


static const char* doc_mean =
R"(mean()
--

Equivalent to :func:`dt.mean(self)`.
)";

oobj PyFExpr::mean(const XArgs&) {
  auto meanFn = oobj::import("datatable", "mean");
  return meanFn.call({this});
}

DECLARE_METHOD(&PyFExpr::mean)
    ->name("mean")
    ->docs(doc_mean);


static const char* doc_median =
R"(median()
--

Equivalent to :func:`dt.median(self)`.
)";

oobj PyFExpr::median(const XArgs&) {
  auto medianFn = oobj::import("datatable", "median");
  return medianFn.call({this});
}

DECLARE_METHOD(&PyFExpr::median)
    ->name("median")
    ->docs(doc_median);


static const char* doc_min =
R"(min()
--

Equivalent to :func:`dt.min(self)`.
)";

oobj PyFExpr::min(const XArgs&) {
  auto minFn = oobj::import("datatable", "min");
  return minFn.call({this});
}

DECLARE_METHOD(&PyFExpr::min)
    ->name("min")
    ->docs(doc_min);
static const char* doc_rowall =
R"(rowall()
--

Equivalent to :func:`dt.rowall(self)`.
)";

oobj PyFExpr::rowall(const XArgs&) {
  auto rowallFn = oobj::import("datatable", "rowall");
  return rowallFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowall)
    ->name("rowall")
    ->docs(doc_rowall);


static const char* doc_rowany =
R"(rowany()
--

Equivalent to :func:`dt.rowany(self)`.
)";

oobj PyFExpr::rowany(const XArgs&) {
  auto rowanyFn = oobj::import("datatable", "rowany");
  return rowanyFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowany)
    ->name("rowany")
    ->docs(doc_rowany);


static const char* doc_rowcount =
R"(rowcount()
--

Equivalent to :func:`dt.rowcount(self)`.
)";

oobj PyFExpr::rowcount(const XArgs&) {
  auto rowcountFn = oobj::import("datatable", "rowcount");
  return rowcountFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowcount)
    ->name("rowcount")
    ->docs(doc_rowcount);


static const char* doc_rowfirst =
R"(rowfirst()
--

Equivalent to :func:`dt.rowfirst(self)`.
)";

oobj PyFExpr::rowfirst(const XArgs&) {
  auto rowfirstFn = oobj::import("datatable", "rowfirst");
  return rowfirstFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowfirst)
    ->name("rowfirst")
    ->docs(doc_rowfirst);


static const char* doc_rowlast =
R"(rowlast()
--

Equivalent to :func:`dt.rowlast(self)`.
)";

oobj PyFExpr::rowlast(const XArgs&) {
  auto rowlastFn = oobj::import("datatable", "rowlast");
  return rowlastFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowlast)
    ->name("rowlast")
    ->docs(doc_rowlast);


static const char* doc_rowmax =
R"(rowmax()
--

Equivalent to :func:`dt.rowmax(self)`.
)";

oobj PyFExpr::rowmax(const XArgs&) {
  auto rowmaxFn = oobj::import("datatable", "rowmax");
  return rowmaxFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowmax)
    ->name("rowmax")
    ->docs(doc_rowmax);


static const char* doc_rowmean =
R"(rowmean()
--

Equivalent to :func:`dt.rowmean(self)`.
)";

oobj PyFExpr::rowmean(const XArgs&) {
  auto rowmeanFn = oobj::import("datatable", "rowmean");
  return rowmeanFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowmean)
    ->name("rowmean")
    ->docs(doc_rowmean);


static const char* doc_rowmin =
R"(rowmin()
--

Equivalent to :func:`dt.rowmin(self)`.
)";

oobj PyFExpr::rowmin(const XArgs&) {
  auto rowminFn = oobj::import("datatable", "rowmin");
  return rowminFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowmin)
    ->name("rowmin")
    ->docs(doc_rowmin);


static const char* doc_rowsd =
R"(rowsd()
--

Equivalent to :func:`dt.rowsd(self)`.
)";

oobj PyFExpr::rowsd(const XArgs&) {
  auto rowsdFn = oobj::import("datatable", "rowsd");
  return rowsdFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowsd)
    ->name("rowsd")
    ->docs(doc_rowsd);


static const char* doc_rowsum =
R"(rowsum()
--

Equivalent to :func:`dt.rowsum(self)`.
)";

oobj PyFExpr::rowsum(const XArgs&) {
  auto rowsumFn = oobj::import("datatable", "rowsum");
  return rowsumFn.call({this});
}

DECLARE_METHOD(&PyFExpr::rowsum)
    ->name("rowsum")
    ->docs(doc_rowsum);


static const char* doc_sd =
R"(sd()
--

Equivalent to :func:`dt.sd(self)`.
)";

oobj PyFExpr::sd(const XArgs&) {
  auto sdFn = oobj::import("datatable", "sd");
  return sdFn.call({this});
}

DECLARE_METHOD(&PyFExpr::sd)
    ->name("sd")
    ->docs(doc_sd);


static const char* doc_shift =
R"(shift(n=1)
--

Equivalent to :func:`dt.shift(self, n)`.
)";

oobj PyFExpr::shift(const XArgs& args) {
  auto shiftFn = oobj::import("datatable", "shift");
  oobj n = args[0]? args[0].to_oobj() : py::oint(1);
  return shiftFn.call({this, n});
}

DECLARE_METHOD(&PyFExpr::shift)
    ->name("shift")
    ->docs(doc_shift)
    ->arg_names({"n"})
    ->n_positional_or_keyword_args(1);

static const char* doc_last =
R"(last()
--

Equivalent to :func:`dt.last(self)`.
)";

oobj PyFExpr::last(const XArgs&) {
  auto lastFn = oobj::import("datatable", "last");
  return lastFn.call({this});
}

DECLARE_METHOD(&PyFExpr::last)
    ->name("last")
    ->docs(doc_last);


static const char* doc_count =
R"(count()
--

Equivalent to :func:`dt.count(self)`.
)";

oobj PyFExpr::count(const XArgs&) {
  auto countFn = oobj::import("datatable", "count");
  return countFn.call({this});
}

DECLARE_METHOD(&PyFExpr::count)
    ->name("count")
    ->docs(doc_count);


static const char* doc_first =
R"(first()
--

Equivalent to :func:`dt.first(self)`.
)";

oobj PyFExpr::first(const XArgs&) {
  auto firstFn = oobj::import("datatable", "first");
  return firstFn.call({this});
}

DECLARE_METHOD(&PyFExpr::first)
    ->name("first")
    ->docs(doc_first);


//------------------------------------------------------------------------------
// Class decoration
//------------------------------------------------------------------------------

static const char* doc_fexpr =
R"(
FExpr is an object that encapsulates computations to be done on a frame.

FExpr objects are rarely constructed directly (though it is possible too),
instead they are more commonly created as inputs/outputs from various
functions in :mod:`datatable`.

Consider the following example::

    math.sin(2 * f.Angle)

Here accessing column "Angle" in namespace ``f`` creates an ``FExpr``.
Multiplying this ``FExpr`` by a python scalar ``2`` creates a new ``FExpr``.
And finally, applying the sine function creates yet another ``FExpr``. The
resulting expression can be applied to a frame via the
:meth:`DT[i,j] <dt.Frame.__getitem__>` method, which will compute that expression
using the data of that particular frame.

Thus, an ``FExpr`` is a stored computation, which can later be applied to a
Frame, or to multiple frames.

Because of its delayed nature, an ``FExpr`` checks its correctness at the time
when it is applied to a frame, not sooner. In particular, it is possible for
the same expression to work with one frame, but fail with another. In the
example above, the expression may raise an error if there is no column named
"Angle" in the frame, or if the column exists but has non-numeric type.

Most functions in datatable that accept an ``FExpr`` as an input, return
a new ``FExpr`` as an output, thus creating a tree of ``FExpr``s as the
resulting evaluation graph.

Also, all functions that accept ``FExpr``s as arguments, will also accept
certain other python types as an input, essentially converting them into
``FExpr``s. Thus, we will sometimes say that a function accepts **FExpr-like**
objects as arguments.

)";

void PyFExpr::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.FExpr");
  xt.set_class_doc(doc_fexpr);
  xt.set_subclassable(false);

  xt.add(CONSTRUCTOR(&PyFExpr::m__init__, args__init__));
  xt.add(DESTRUCTOR(&PyFExpr::m__dealloc__));
  xt.add(METHOD(&PyFExpr::extend, args_extend));
  xt.add(METHOD(&PyFExpr::remove, args_remove));
  xt.add(METHOD0(&PyFExpr::len, "len"));
  xt.add(METHOD(&PyFExpr::re_match, args_re_match));

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
