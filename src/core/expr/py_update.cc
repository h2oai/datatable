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
#include "expr/py_update.h"
namespace py {



//------------------------------------------------------------------------------
// oupdate_pyobject
//------------------------------------------------------------------------------

static const char* doc_update =
R"(update(**kwargs)
--

Create new or update existing columns within a frame.

This expression is intended to be used at "j" place in ``DT[i, j]``
call. It takes an arbitrary number of key/value pairs each describing
a column name and the expression for how that column has to be
created/updated.

Examples
--------
.. code-block:: python

    >>> from datatable import dt, f, by, update
    >>>
    >>> DT = dt.Frame([range(5), [4, 3, 9, 11, -1]], names=("A", "B"))
    >>> DT
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     0      4
     1 |     1      3
     2 |     2      9
     3 |     3     11
     4 |     4     -1
    [5 rows x 2 columns]

Create new columns and update existing columns::

    >>> DT[:, update(C = f.A * 2,
    ...              D = f.B // 3,
    ...              A = f.A * 4,
    ...              B = f.B + 1)]
    >>> DT
       |     A      B      C      D
       | int32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 |     0      5      0      1
     1 |     4      4      2      1
     2 |     8     10      4      3
     3 |    12     12      6      3
     4 |    16      0      8     -1
    [5 rows x 4 columns]

Add new column with `unpacking`_; this can be handy for dynamicallly adding
columns with dictionary comprehensions, or if the names are not valid python
keywords::

    >>> DT[:, update(**{"extra column": f.A + f.B + f.C + f.D})]
    >>> DT
       |     A      B      C      D  extra column
       | int32  int32  int32  int32         int32
    -- + -----  -----  -----  -----  ------------
     0 |     0      5      0      1             6
     1 |     4      4      2      1            11
     2 |     8     10      4      3            25
     3 |    12     12      6      3            33
     4 |    16      0      8     -1            23
    [5 rows x 5 columns]

You can update a subset of data::

    >>> DT[f.A > 10, update(A = f.A * 5)]
    >>> DT
       |     A      B      C      D  extra column
       | int32  int32  int32  int32         int32
    -- + -----  -----  -----  -----  ------------
     0 |     0      5      0      1             6
     1 |     4      4      2      1            11
     2 |     8     10      4      3            25
     3 |    60     12      6      3            33
     4 |    80      0      8     -1            23
    [5 rows x 5 columns]

You can also add a new column or update an existing column in a groupby
operation, similar to SQL's `window` operation, or pandas `transform()`::

    >>> df = dt.Frame("""exporter assets   liabilities
    ...                   False      5          1
    ...                   True       10         8
    ...                   False      3          1
    ...                   False      24         20
    ...                   False      40         2
    ...                   True       12         11""")
    >>>
    >>> # Get the ratio for each row per group
    >>> df[:,
    ...    update(ratio = dt.sum(f.liabilities) * 100 / dt.sum(f.assets)),
    ...    by(f.exporter)]
    >>> df
       | exporter  assets  liabilities    ratio
       |    bool8   int32        int32  float64
    -- + --------  ------  -----------  -------
     0 |        0       5            1  33.3333
     1 |        1      10            8  86.3636
     2 |        0       3            1  33.3333
     3 |        0      24           20  33.3333
     4 |        0      40            2  33.3333
     5 |        1      12           11  86.3636
    [6 rows x 4 columns]


.. _`unpacking` : https://docs.python.org/3/tutorial/controlflow.html#unpacking-argument-lists
)";

static PKArgs args___init__(0, 0, 0, false, true, {}, "__init__", doc_update);


void oupdate::oupdate_pyobject::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.update");
  xt.set_class_doc("update() clause for use in DT[i, j, ...]");
  xt.set_subclassable(false);

  xt.add(CONSTRUCTOR(&oupdate::oupdate_pyobject::m__init__, args___init__));
  xt.add(DESTRUCTOR(&oupdate::oupdate_pyobject::m__dealloc__));

  static GSArgs args__names("_names");
  static GSArgs args__exprs("_exprs");
  xt.add(GETTER(&oupdate::oupdate_pyobject::get_names, args__names));
  xt.add(GETTER(&oupdate::oupdate_pyobject::get_exprs, args__exprs));
}


void oupdate::oupdate_pyobject::m__init__(const PKArgs& args) {
  size_t n = args.num_varkwd_args();
  names_ = py::olist(n);
  exprs_ = py::olist(n);
  size_t i = 0;
  for (auto kw : args.varkwds()) {
    names_.set(i, kw.first);
    exprs_.set(i, kw.second);
    i++;
  }
}


void oupdate::oupdate_pyobject::m__dealloc__() {
  names_ = py::olist();
  exprs_ = py::olist();
}


oobj oupdate::oupdate_pyobject::get_names() const {
  return names_;
}

oobj oupdate::oupdate_pyobject::get_exprs() const {
  return exprs_;
}




//------------------------------------------------------------------------------
// oupdate
//------------------------------------------------------------------------------

oupdate::oupdate(const robj& r) : oobj(r) {
  xassert(check(v));
}


bool oupdate::check(PyObject* val) {
  return oupdate::oupdate_pyobject::check(val);
}


void oupdate::init(PyObject* m) {
  oupdate::oupdate_pyobject::init_type(m);
}


oobj oupdate::get_names() const {
  return reinterpret_cast<const oupdate::oupdate_pyobject*>(v)->get_names();
}

oobj oupdate::get_exprs() const {
  return reinterpret_cast<const oupdate::oupdate_pyobject*>(v)->get_exprs();
}




}  // namespace py
