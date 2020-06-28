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
#include "str/py_str.h"
#include "datatablemodule.h"
#include "frame/py_frame.h"
#include "stype.h"
#include "utils/exceptions.h"
namespace py {


static PKArgs args_split_into_nhot(
    1, 0, 2, false, false,
    {"frame", "sep", "sort"}, "split_into_nhot", nullptr
);


static oobj split_into_nhot(const PKArgs& args) {
  if (args[0].is_undefined()) {
    throw ValueError() << "Required parameter `frame` is missing";
  }

  if (args[0].is_none()) {
    return py::None();
  }

  DataTable* dt = args[0].to_datatable();
  std::string sep = args[1]? args[1].to_string() : ",";
  bool sort = args[2]? args[2].to_bool_strict() : false;

  if (dt->ncols() != 1) {
    throw ValueError() << "Function split_into_nhot() may only be applied to "
      "a single-column Frame of type string;" << " got frame with "
      << dt->ncols() << " columns";
  }
  const Column& col0 = dt->get_column(0);
  dt::SType st = col0.stype();
  if (!(st == dt::SType::STR32 || st == dt::SType::STR64)) {
    throw TypeError() << "Function split_into_nhot() may only be applied to "
      "a single-column Frame of type string;" << " received a column of type "
      << st;
  }
  if (sep.size() != 1) {
    throw ValueError() << "Parameter `sep` in split_into_nhot() must be a "
      "single character; got '" << sep << "'";
  }

  DataTable* res = dt::split_into_nhot(col0, sep[0], sort);
  return Frame::oframe(res);
}


void DatatableModule::init_methods_str() {
  ADD_FN(&split_into_nhot, args_split_into_nhot);
}

} // namespace py
