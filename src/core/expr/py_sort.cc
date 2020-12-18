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
  PyObject* cls = reinterpret_cast<PyObject*>(&osort::osort_pyobject::type);
  v = PyObject_CallObject(cls, cols.to_borrowed_ref());
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
