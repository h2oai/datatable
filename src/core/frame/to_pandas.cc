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
#include "datatablemodule.h"
#include "documentation.h"
#include "frame/py_frame.h"
#include "python/_all.h"
#include "python/xargs.h"
#include "stype.h"
namespace py {



oobj Frame::to_pandas(const XArgs&) {
  const size_t ncols = dt->ncols();
  const size_t nkeys = dt->nkeys();

  oobj pandas = oobj::import("pandas");
  oobj pd_DataFrame = pandas.get_attr("DataFrame");
  otuple names = dt->get_pynames();

  oobj index = None();
  if (nkeys) {
    oobj pd_Index = pandas.get_attr("Index");
    olist indices(nkeys);
    for (size_t i = 0; i < nkeys; i++) {
      auto column = robj(this).invoke("to_numpy", {None(), oint(i)});
      auto index_item = pd_Index.call({column, None(), None(), names[i]});
      indices.set(i, std::move(index_item));
    }
    if (nkeys == 1) {
      index = indices[0];
    } else {
      index = std::move(indices);
    }
  }

  // Note: data has to be a dict, otherwise pandas creates frame
  //       by rows.
  odict data;
  for (size_t i = nkeys; i < ncols; i++) {
    oobj column;
    if (dt->get_column(i).type().is_void()) {
      olist res { 1 };
      res.set(0, py::None());
      column = res.invoke("__mul__", {py::oint(dt->nrows())});
    } else {
      column = robj(this).invoke("to_numpy", {None(), oint(i)});
    }
    data.set(names[i], column);
  }

  oobj columns = names;
  if (nkeys) {
    columns = names.invoke("__getitem__", {
        oslice(static_cast<int64_t>(nkeys), oslice::NA, oslice::NA)
    });
  }

  return pd_DataFrame.call({data, index, columns});
}


DECLARE_METHOD(&Frame::to_pandas)
    ->name("to_pandas")
    ->docs(dt::doc_Frame_to_pandas);




}  // namespace py::
