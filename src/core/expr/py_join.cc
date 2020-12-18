//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#include "expr/py_join.h"
#include "datatable.h"
#include "python/arg.h"
namespace py {



//------------------------------------------------------------------------------
// ojoin::pyobj
//------------------------------------------------------------------------------

static const char* doc_join =
R"(join(frame)
--

Join clause for use in Frame’s square-bracket selector.

This clause is equivalent to the SQL `JOIN`, though for the moment
datatable only supports left outer joins. In order to join,
the `frame` must be :attr:`keyed <dt.Frame.key>` first, and then joined
to another frame `DT` as::

    >>> DT[:, :, join(X)]

provided that `DT` has the column(s) with the same name(s) as
the key in `frame`.

Parameters
----------
frame: Frame
    An input keyed frame to be joined to the current one.

return: Join Object
    In most of the cases the returned object is directly used in the
    Frame’s square-bracket selector.

except: ValueError
    The exception is raised if `frame` is not keyed.

See Also
--------
- :ref:`Tutorial on joins <join tutorial>`

Examples
--------
.. code-block:: python

    >>> df1 = dt.Frame("""    date    X1  X2
    ...                   01-01-2020  H   10
    ...                   01-02-2020  H   30
    ...                   01-03-2020  Y   15
    ...                   01-04-2020  Y   20""")
    >>>
    >>> df2 = dt.Frame("""X1  X3
    ...                   H   5
    ...                   Y   10""")


First, create a key on the right frame (``df2``). Note that the join key
(``X1``) has unique values and has the same name in the left frame (``df1``)::

    >>> df2.key = "X1"

Join is now possible::

    >>> df1[:, :, join(df2)]
       | date        X1        X2     X3
       | str32       str32  int32  int32
    -- + ----------  -----  -----  -----
     0 | 01-01-2020  H         10      5
     1 | 01-02-2020  H         30      5
     2 | 01-03-2020  Y         15     10
     3 | 01-04-2020  Y         20     10
    [4 rows x 4 columns]

You can refer to columns of the joined frame using prefix :data:`g. <dt.g>`, similar to how columns of the left frame can be accessed using prefix :data:`f. <dt.f>`::

    >>> df1[:, update(X2=f.X2 * g.X3), join(df2)]
    >>> df1
       | date        X1        X2
       | str32       str32  int32
    -- + ----------  -----  -----
     0 | 01-01-2020  H         50
     1 | 01-02-2020  H        150
     2 | 01-03-2020  Y        150
     3 | 01-04-2020  Y        200
    [4 rows x 3 columns]
)";

static PKArgs args___init__(
    1, 0, 0, false, false, {"frame"}, "__init__", doc_join);

void ojoin::pyobj::m__init__(const PKArgs& args) {
  if (!args[0]) {
    throw TypeError() << "join() is missing the required parameter `frame`";
  }
  join_frame = args[0].to_oobj();
  if (!join_frame.is_frame()) {
    throw TypeError() << "The argument to join() must be a Frame";
  }
  DataTable* jdt = join_frame.to_datatable();
  if (jdt->nkeys() == 0) {
    throw ValueError() << "The join frame is not keyed";
  }
}


void ojoin::pyobj::m__dealloc__() {
  join_frame = nullptr;  // Releases the stored oobj
}


oobj ojoin::pyobj::get_joinframe() const {
  return join_frame;
}

void ojoin::pyobj::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.join");
  xt.set_class_doc("join() clause for use in DT[i, j, ...]");
  xt.set_subclassable(true);

  static GSArgs args_joinframe("joinframe");
  xt.add(CONSTRUCTOR(&pyobj::m__init__, args___init__));
  xt.add(DESTRUCTOR(&pyobj::m__dealloc__));
  xt.add(GETTER(&pyobj::get_joinframe, args_joinframe));
}



//------------------------------------------------------------------------------
// ojoin
//------------------------------------------------------------------------------

ojoin::ojoin(const robj& src) : oobj(src) {}


DataTable* ojoin::get_datatable() const {
  auto w = static_cast<pyobj*>(v);
  return w->join_frame.to_datatable();
}


bool ojoin::check(PyObject* val) {
  return pyobj::check(val);
}


void ojoin::init(PyObject* m) {
  pyobj::init_type(m);
}



}  // namespace py
