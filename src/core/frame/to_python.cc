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
#include "frame/py_frame.h"
#include "python/_all.h"
#include "python/args.h"
#include "python/string.h"
#include "python/tuple.h"
namespace py {



//------------------------------------------------------------------------------
// to_tuples()
//------------------------------------------------------------------------------

static const char* doc_to_tuples =
R"(to_tuples(self)
--

Convert the frame into a list of tuples, by rows.

Parameters
----------
return: List[Tuple]
  Returns a list having :attr:`nrows <Frame.nrows>` tuples, where
  each tuple has length :attr:`ncols <Frame.ncols>` and contains data
  from each respective row of the frame.

Examples
--------
>>> DT = dt.Frame(A=[1, 2, 3], B=["aye", "nay", "tain"])
>>> DT.to_tuples()
[(1, "aye"), (2, "nay"), (3, "tain")]
)";

static PKArgs args_to_tuples(
  0, 0, 0, false, false, {}, "to_tuples", doc_to_tuples);


oobj Frame::to_tuples(const PKArgs&) {
  std::vector<py::otuple> list_of_tuples;
  for (size_t i = 0; i < dt->nrows(); ++i) {
    list_of_tuples.push_back(py::otuple(dt->ncols()));
  }
  for (size_t j = 0; j < dt->ncols(); ++j) {
    const Column& col = dt->get_column(j);
    for (size_t i = 0; i < dt->nrows(); ++i) {
      list_of_tuples[i].set(j, col.get_element_as_pyobject(i));
    }
  }
  py::olist res(dt->nrows());
  for (size_t i = 0; i < dt->nrows(); ++i) {
    res.set(i, std::move(list_of_tuples[i]));
  }
  return std::move(res);
}



//------------------------------------------------------------------------------
// to_list()
//------------------------------------------------------------------------------

static const char* doc_to_list =
R"(to_list(self)
--

Convert the frame into a list of lists, by columns.


Parameters
----------
return: List[List]
    A list of :attr:`ncols <Frame.ncols>` lists, each inner list
    representing one column of the frame.


Examples
--------
>>> DT = dt.Frame(A=[1, 2, 3], B=["aye", "nay", "tain"])
>>> DT.to_list()
[[1, 2, 3], ["aye", "nay", "tain"]]

>>> dt.Frame(id=range(10)).to_list()
[[0, 1, 2, 3, 4, 5, 6, 7, 8, 9]]
)";

static PKArgs args_to_list(
  0, 0, 0, false, false, {}, "to_list", doc_to_list);

oobj Frame::to_list(const PKArgs&) {
  py::olist res(dt->ncols());
  for (size_t j = 0; j < dt->ncols(); ++j) {
    py::olist pycol(dt->nrows());
    const Column& col = dt->get_column(j);
    for (size_t i = 0; i < dt->nrows(); ++i) {
      pycol.set(i, col.get_element_as_pyobject(i));
    }
    res.set(j, std::move(pycol));
  }
  return std::move(res);
}



//------------------------------------------------------------------------------
// to_list()
//------------------------------------------------------------------------------

static const char* doc_to_dict =
R"(to_dict(self)
--

Convert the frame into a dictionary of lists, by columns.

In Python 3.6+ the order of records in the dictionary will be the
same as the order of columns in the frame.

Parameters
----------
return: Dict[str, List]
    Dictionary with :attr:`ncols <Frame.ncols>` records. Each record
    represents a single column: the key is the column's name, and the
    value is the list with the column's data.

Examples
--------
>>> DT = dt.Frame(A=[1, 2, 3], B=["aye", "nay", "tain"])
>>> DT.to_dict()
{"A": [1, 2, 3], "B": ["aye", "nay", "tain"]}

See also
--------
- :meth:`.to_list`: convert the frame into a list of lists
- :meth:`.to_tuples`: convert the frame into a list of tuples by rows
)";

static PKArgs args_to_dict(
  0, 0, 0, false, false, {}, "to_dict", doc_to_dict);

oobj Frame::to_dict(const PKArgs&) {
  py::otuple names = dt->get_pynames();
  py::odict res;
  for (size_t j = 0; j < dt->ncols(); ++j) {
    py::olist pycol(dt->nrows());
    const Column& col = dt->get_column(j);
    for (size_t i = 0; i < dt->nrows(); ++i) {
      pycol.set(i, col.get_element_as_pyobject(i));
    }
    res.set(names[j], pycol);
  }
  return std::move(res);
}



void Frame::_init_topython(XTypeMaker& xt) {
  xt.add(METHOD(&Frame::to_dict, args_to_dict));
  xt.add(METHOD(&Frame::to_list, args_to_list));
  xt.add(METHOD(&Frame::to_tuples, args_to_tuples));
}



}  // namespace py
