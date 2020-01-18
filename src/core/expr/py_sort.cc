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
#include "expr/py_sort.h"
#include "python/_all.h"
#include "utils/exceptions.h"
namespace py {



//------------------------------------------------------------------------------
// py::osort::osort_pyobject
//------------------------------------------------------------------------------

static const char* sort_help =
R"(sort(*cols, reverse=False)

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
applied after the sorting. For example,::

  DT[:10, :, sort(f.Highscore, reverse=True)]

will select the first 10 records from the frame ``DT`` ordered by
the Highscore column.
)";

static PKArgs args___init__(
  0, 0, 1, true, false, {"reverse"}, "__init__", nullptr
);

void osort::osort_pyobject::m__init__(const PKArgs& args)
{
  const Arg& arg_reverse = args[0];

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
  reverse_ = nullptr;
  cols_ = nullptr;  // Releases the stored oobj
}


oobj osort::osort_pyobject::get_cols() const {
  return cols_;
}


const std::vector<bool>& osort::osort_pyobject::get_reverse() const {
  return *reverse_;
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
  PyObject* cls = reinterpret_cast<PyObject*>(&osort::osort_pyobject::type);
  v = PyObject_CallObject(cls, cols.to_borrowed_ref());
  if (!v) throw PyError();
}


bool osort::check(PyObject* v) {
  return osort::osort_pyobject::check(v);
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



}  // namespace py
