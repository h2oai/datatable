//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include "expr/py_sort.h"
#include "python/_all.h"
#include "utils/exceptions.h"
namespace py {

static NaPosition  get_na_position_from_string(const std::string& str) {
   return (str == "first")? NaPosition::FIRST :
          (str == "last")? NaPosition::LAST :
          (str == "remove")? NaPosition::REMOVE : NaPosition::INVALID;
}

//------------------------------------------------------------------------------
// py::osort::osort_pyobject
//------------------------------------------------------------------------------

static const char* sort_help =
R"(sort(*cols, reverse=False)
--

Sort clause for use in Frame's square-bracket selector.

When a ``sort()`` object is present inside a ``DT[i, j, ...]``
expression, it will sort the rows of the resulting Frame according
to the columns ``cols`` passed as the arguments to ``sort()``.

When used together with ``by()``, the sort clause applies after the
group-by, i.e. we sort elements within each group. Note, however,
that because we use stable sorting, the operations of grouping and
sorting are commutative: the result of applying groupby and then sort
is the same as the result of sorting first and then doing groupby.

When used together with ``i`` (row filter), the ``i`` filter is
applied after the sorting. For example::

    >>> DT[:10, :, sort(f.Highscore, reverse=True)]

will select the first 10 records from the frame ``DT`` ordered by
the Highscore column.

Examples
--------
.. code-block:: python

    >>> from datatable import dt, f, by
    >>>
    >>> DT = dt.Frame({"col1": ["A", "A", "B", None, "D", "C"],
    >>>                "col2": [2, 1, 9, 8, 7, 4],
    >>>                "col3": [0, 1, 9, 4, 2, 3],
    >>>                "col4": [1, 2, 3, 3, 2, 1]})
    >>>
    >>> DT
       | col1    col2   col3   col4
       | str32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 | A          2      0      1
     1 | A          1      1      2
     2 | B          9      9      3
     3 | NA         8      4      3
     4 | D          7      2      2
     5 | C          4      3      1
    [6 rows x 4 columns]

Sort by a single column::

    >>> DT[:, :, dt.sort("col1")]
       | col1    col2   col3   col4
       | str32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 | NA         8      4      3
     1 | A          2      0      1
     2 | A          1      1      2
     3 | B          9      9      3
     4 | C          4      3      1
     5 | D          7      2      2
    [6 rows x 4 columns]


Sort by multiple columns::

    >>> DT[:, :, dt.sort("col2", "col3")]
       | col1    col2   col3   col4
       | str32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 | A          1      1      2
     1 | A          2      0      1
     2 | C          4      3      1
     3 | D          7      2      2
     4 | NA         8      4      3
     5 | B          9      9      3
    [6 rows x 4 columns]

Sort in descending order::

    >>> DT[:, :, dt.sort(-f.col1)]
       | col1    col2   col3   col4
       | str32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 | NA         8      4      3
     1 | D          7      2      2
     2 | C          4      3      1
     3 | B          9      9      3
     4 | A          2      0      1
     5 | A          1      1      2
    [6 rows x 4 columns]

The frame can also be sorted in descending order by setting the ``reverse`` parameter to ``True``::

    >>> DT[:, :, dt.sort("col1", reverse=True)]
       | col1    col2   col3   col4
       | str32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 | NA         8      4      3
     1 | D          7      2      2
     2 | C          4      3      1
     3 | B          9      9      3
     4 | A          2      0      1
     5 | A          1      1      2
    [6 rows x 4 columns]

By default, when sorting, null values are placed at the top; to relocate null values to the bottom, pass ``last`` to the ``na_position`` parameter::

    >>> DT[:, :, dt.sort("col1", na_position="last")]
       | col1    col2   col3   col4
       | str32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 | A          2      0      1
     1 | A          1      1      2
     2 | B          9      9      3
     3 | C          4      3      1
     4 | D          7      2      2
     5 | NA         8      4      3
    [6 rows x 4 columns]

Passing ``remove`` to ``na_position`` completely excludes any row with null values from the sorted output:

    >>> DT[:, :, dt.sort("col1", na_position="remove")]
       | col1    col2   col3   col4
       | str32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 | A          2      0      1
     1 | A          1      1      2
     2 | B          9      9      3
     3 | C          4      3      1
     4 | D          7      2      2
    [5 rows x 4 columns]

Sort by multiple columns, descending and ascending order::

    >>> DT[:, :, dt.sort(-f.col2, f.col3)]
       | col1    col2   col3   col4
       | str32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 | B          9      9      3
     1 | NA         8      4      3
     2 | D          7      2      2
     3 | C          4      3      1
     4 | A          2      0      1
     5 | A          1      1      2
    [6 rows x 4 columns]

The same code above can be replicated by passing a list of booleans to ``reverse``::

    >>> DT[:, :, dt.sort("col2", "col3", reverse=[True, False])]
       | col1    col2   col3   col4
       | str32  int32  int32  int32
    -- + -----  -----  -----  -----
     0 | B          9      9      3
     1 | NA         8      4      3
     2 | D          7      2      2
     3 | C          4      3      1
     4 | A          2      0      1
     5 | A          1      1      2
    [6 rows x 4 columns]

In the presence of :func:`by()`, :func:`sort()` sorts within each group::

    >>> DT[:, :, by("col4"), dt.sort(f.col2)]
       |  col4  col1    col2   col3
       | int32  str32  int32  int32
    -- + -----  -----  -----  -----
     0 |     1  A          2      0
     1 |     1  C          4      3
     2 |     2  A          1      1
     3 |     2  D          7      2
     4 |     3  NA         8      4
     5 |     3  B          9      9
    [6 rows x 4 columns]


)";

static PKArgs args___init__(
  0, 0, 2, true, false, {"reverse", "na_position"}, "__init__", nullptr
);

void osort::osort_pyobject::m__init__(const PKArgs& args)
{
  const Arg& arg_reverse = args[0];
  const Arg& arg_na_position = args[1];

  if (arg_reverse.is_none_or_undefined()) {
    reverse_ = new std::vector<bool>();
  }
  else if (arg_reverse.is_bool()) {
    bool rev = arg_reverse.to<bool>(false);
    reverse_ = new std::vector<bool>(1, rev);
  }
  else if (arg_reverse.is_list_or_tuple()) {
    auto revlist = arg_reverse.to_pylist();
    reverse_ = new std::vector<bool>(revlist.size());
    for (size_t i = 0; i < reverse_->size(); ++i) {
      (*reverse_)[i] = revlist[i].to_bool_strict();
    }
  }
  else {
    throw TypeError() << arg_reverse.name() << " should be a boolean or a list "
        "of booleans, instead got " << arg_reverse.typeobj();
  }

  if (arg_na_position.is_none_or_undefined()) {
    na_position_ = new std::vector<NaPosition>(1, NaPosition::FIRST);
  }
  else if (arg_na_position.is_string()) {
    NaPosition na_pos = get_na_position_from_string(arg_na_position.to_string());
    if (na_pos == NaPosition::INVALID) {
      throw ValueError() << "na position value `" << arg_na_position.to_string() << "` is not supported";
    }
    na_position_ = new std::vector<NaPosition>(1, na_pos);
  }
  /*else if (arg_na_position.is_list_or_tuple()) {  ######## This needs to be implemented ################
    auto na_position_list  = arg_na_position.to_pylist();
    na_position_ = new std::vector<NaPosition>(na_position_list.size());
    for (size_t i = 0; i < na_position_->size(); ++i) {
      NaPosition na_pos = get_na_position_from_string(na_position_list[i].to_string());
      if (na_pos == NaPosition::INVALID) {
        throw ValueError() << "na position value `" << na_position_list[i].to_string() << "` is not supported";
      }
      (*na_position_)[i] = na_pos;
    }
  }*/
  else {
    throw TypeError() << arg_na_position.name() <<
        " should be one of 'first', 'last' or 'remove', instead got " << arg_na_position.typeobj();
  }

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


void osort::osort_pyobject::m__dealloc__() {
  delete reverse_;
  delete na_position_;
  reverse_ = nullptr;
  cols_ = nullptr;  // Releases the stored oobj
  na_position_ = nullptr;
}


oobj osort::osort_pyobject::get_cols() const {
  return cols_;
}


const std::vector<bool>& osort::osort_pyobject::get_reverse() const {
  return *reverse_;
}

const std::vector<NaPosition>& osort::osort_pyobject::get_na_position() const {
  return *na_position_;
}

void osort::osort_pyobject::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.sort");
  xt.set_class_doc(sort_help);

  xt.add(CONSTRUCTOR(&osort::osort_pyobject::m__init__, args___init__));
  xt.add(DESTRUCTOR(&osort::osort_pyobject::m__dealloc__));
}




//------------------------------------------------------------------------------
// py::osort
//------------------------------------------------------------------------------

osort::osort(const robj& src) : oobj(src) {}
osort::osort(const oobj& src) : oobj(src) {}

osort::osort(const otuple& cols) {
  v = PyObject_CallObject(osort::osort_pyobject::typePtr,
                          cols.to_borrowed_ref());
  if (!v) throw PyError();
}


bool osort::check(PyObject* val) {
  return osort::osort_pyobject::check(val);
}


void osort::init(PyObject* m) {
  osort::osort_pyobject::init_type(m);
}


oobj osort::get_arguments() const {
  return reinterpret_cast<const osort::osort_pyobject*>(v)->get_cols();
}

const std::vector<bool>& osort::get_reverse() const {
  return reinterpret_cast<const osort::osort_pyobject*>(v)->get_reverse();
}

const std::vector<NaPosition>& osort::get_na_position() const {
  return reinterpret_cast<const osort::osort_pyobject*>(v)->get_na_position();
}

}  // namespace py
