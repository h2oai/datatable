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
  // ```
  // from pandas import DataFrame
  // names = self.names
  // ```
  oobj pandas = oobj::import("pandas");
  oobj dataframe = pandas.get_attr("DataFrame");
  otuple names = dt->get_pynames();

  // ```
  // cols = {names[i]: self.to_numpy(None, i)
  //         for i in range(self.ncols)}
  // ```
  odict cols;
  otuple np_call_args(2);
  np_call_args.set(0, py::None());
  for (size_t i = 0; i < dt->ncols(); ++i) {
    np_call_args.replace(1, oint(i));
    cols.set(names[i],
             robj(this).invoke("to_numpy", otuple(np_call_args)));
  }
  // ```
  // return DataFrame(cols, columns=names)
  // ```
  odict pd_call_kws;
  pd_call_kws.set(ostring("columns"), names);
  return dataframe.call(otuple(cols), pd_call_kws);
}


void Frame::_init_to_pandas(XTypeMaker& xt) {
  xt.add(METHOD(&Frame::to_pandas, args_to_pandas));
}




}  // namespace py::
