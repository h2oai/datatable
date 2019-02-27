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
    1, 0, 1, false, false,
    {"col", "sep"}, "split_into_nhot", nullptr
);


static oobj split_into_nhot(const PKArgs& args) {
  DataTable* dt = args[0].to_frame();
  std::string sep = args[1]? args[1].to_string() : ",";

  Column* col0 = dt->ncols == 1? dt->columns[0] : nullptr;
  if (!col0) {
    throw ValueError() << "Function split_into_nhot() may only be applied to "
      "a single-column Frame of type string;" << " got frame with "
      << dt->ncols << " columns";
  }
  SType st = col0->stype();
  if (!(st == SType::STR32 || st == SType::STR64)) {
    throw TypeError() << "Function split_into_nhot() may only be applied to "
      "a single-column Frame of type string;" << " received a column of type "
      << info(st).name();
  }
  if (sep.size() != 1) {
    throw ValueError() << "Parameter `sep` in split_into_nhot() must be a "
      "single character; got '" << sep << "'";
  }

  DataTable* res = dt::split_into_nhot(col0, sep[0]);
  return Frame::from_datatable(res);
}



void DatatableModule::init_methods_str() {
  ADD_FN(&split_into_nhot, args_split_into_nhot);
}

} // namespace py
