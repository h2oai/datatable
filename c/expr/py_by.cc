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
#include "expr/py_by.h"
namespace py {



//------------------------------------------------------------------------------
// oby_pyobject
//------------------------------------------------------------------------------

static const char* by_help =
R"(by(*cols, add_columns=True)

Group-by clause for use in Frame's square-bracket selector.

Whenever a ``by()`` object is present inside a ``DT[i, j, ...]``
expression, it makes all other expressions to be evaluated in
group-by mode. This mode causes the following changes to the
evaluation semantics:

- A "Groupby" object will be computed for the frame ``DT``, grouping
  it by columns specified as the arguments to the ``by()`` call. This
  object keeps track of which rows of the frame belong to which group.

- If an ``i`` expression is present (row filter), it will be
  interpreted within each group. For example, if ``i`` is a slice,
  then the slice will be applied separately to each group. Similarly,
  if ``i`` expression contains a formula with reduce functions, then
  those functions will be evaluated for each group. For example::

    DT[f.A == max(f.A), :, by(f.group_id)]

  will select those rows where column A reaches its peak value within
  each group (there could be multiple such rows within each group).

- Before ``j`` is evaluated, the ``by()`` clause adds all its columns
  at the start of ``j`` (unless ``add_columns`` argument is False). If
  ``j`` is a "select-all" slice (i.e. ``:``), then those columns will
  also be excluded from the list of all columns so that they will be
  present in the output only once.

- During evaluation of ``j``, the reducer functions, such as
  :func:`min`, :func:`sum`, etc, will be evaluated by-group, that is
  they will find the minimal value in each group, the sum of values in
  each group, and so on. If a reducer expression is combined with a
  regular column expression, then the reduced column will be
  auto-expanded into a column that is constant within each group.

- Note that if both ``i`` and ``j`` contain reducer functions, then
  those functions will have slightly different notion of groups: the
  reducers in ``i`` will see each group "in full", whereas the
  reducers in ``j`` will see each group after it was filtered by the
  expression in ``i`` (and possibly not even see some of the groups
  at all, if they were filtered out completely).

- If ``j`` contains only reducer expressions, then the final result
  will be a Frame containing containing just a single row for each
  group. This resulting frame will also be keyed by the grouped-by
  columns.


The ``by()`` function expects a single column or a sequence of columns
as the argument(s). It accepts either a column name, or an
f-expression. In particular, you can perform a group-by on a
dynamically computed expression::

  DT[:, :, by(dt.math.floor(f.A/100))]

The default behavior of groupby is to sort the groups in the ascending
order, with NA values appearing before any other values. As a special
case, if you group by an expression ``-f.A``, then it will be
treated as if you requested to group by the column "A" sorting it in
the descending order. This will work even with column types that are
not arithmetic, for example "A" could be a string column here.
)";


static PKArgs args___init__(
  0, 0, 1, true, false, {"add_columns"}, "__init__", nullptr
);

void oby::oby_pyobject::m__init__(const PKArgs& args)
{
  const Arg& arg_add_columns = args[0];
  add_columns_ = arg_add_columns.to<bool>(true);

  size_t n = args.num_vararg_args();
  size_t i = 0;
  olist colslist(n);
  for (robj arg : args.varargs()) {
    colslist.set(i++, arg);
  }
  xassert(i == n);
  if (n == 1 && colslist[0].is_list_or_tuple()) {
    cols_ = colslist[0];
  } else {
    cols_ = std::move(colslist);
  }
}


void oby::oby_pyobject::m__dealloc__() {
  cols_ = nullptr;  // Releases the stored oobj
}


oobj oby::oby_pyobject::get_cols() const {
  return cols_;
}

bool oby::oby_pyobject::get_add_columns() const {
  return add_columns_;
}


void oby::oby_pyobject::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.by");
  xt.set_class_doc(by_help);
  xt.set_subclassable(false);

  xt.add(CONSTRUCTOR(&oby::oby_pyobject::m__init__, args___init__));
  xt.add(DESTRUCTOR(&oby::oby_pyobject::m__dealloc__));
}




//------------------------------------------------------------------------------
// oby
//------------------------------------------------------------------------------

oby::oby(const robj& src) : oobj(src) {}
oby::oby(const oobj& src) : oobj(src) {}


oby oby::make(const robj& r) {
  return oby(oby::oby_pyobject::make(r));
}


bool oby::check(PyObject* v) {
  return oby::oby_pyobject::check(v);
}


void oby::init(PyObject* m) {
  oby::oby_pyobject::init_type(m);
}


oobj oby::get_arguments() const {
  return reinterpret_cast<const oby::oby_pyobject*>(v)->get_cols();
}

bool oby::get_add_columns() const {
  return reinterpret_cast<const oby::oby_pyobject*>(v)->get_add_columns();
}




} // namespace py
