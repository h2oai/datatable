//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//------------------------------------------------------------------------------
#include "str/py_str.h"
#include "datatablemodule.h"
#include "frame/py_frame.h"
#include "types.h"
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
  SType st = col0.stype();
  if (!(st == SType::STR32 || st == SType::STR64)) {
    throw TypeError() << "Function split_into_nhot() may only be applied to "
      "a single-column Frame of type string;" << " received a column of type "
      << info(st).name();
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
