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
#include "frame/py_frame.h"
#include "python/_all.h"
#include "stype.h"
namespace py {



static const char* doc_to_pandas =
R"(to_pandas(self)
--

Convert this frame to a pandas DataFrame.

Parameters
----------
return: pandas.DataFrame

except: ImportError
    If the `pandas` module is not installed.
)";

static PKArgs args_to_pandas(
    0, 0, 0, false, false, {}, "to_pandas", doc_to_pandas);


oobj Frame::to_pandas(const PKArgs&) {
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
    data.set(
        names[i],
        robj(this).invoke("to_numpy", {None(), oint(i)})
    );
  }

  oobj columns = names;
  if (nkeys) {
    columns = names.invoke("__getitem__", {
        oslice(static_cast<int64_t>(nkeys), oslice::NA, oslice::NA)
    });
  }

  return pd_DataFrame.call({data, index, columns});
}


void Frame::_init_to_pandas(XTypeMaker& xt) {
  xt.add(METHOD(&Frame::to_pandas, args_to_pandas));
}




}  // namespace py::
