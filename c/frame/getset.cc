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
#include "frame/py_frame.h"
#include "python/tuple.h"

namespace py {


oobj Frame::m__getitem__(const obj item) const {

  otuple args(5);
  args.set(0, py::obj(this));
  if (item.is_tuple()) {
    otuple argslist = item.to_pytuple();
    size_t n = argslist.size();
    if (n == 1) {
      args.set(1, py::None());
      args.set(2, argslist[0]);
    }
    else if (n >= 2) {
      args.set(1, argslist[0]);
      args.set(2, argslist[1]);
      if (n == 3) {
        args.set(3, argslist[2]);
      } else if (n >= 4) {
        throw ValueError() << "Selector " << item << " is not supported";
      }
    }
  } else {
    args.set(1, py::None());
    args.set(2, item);
  }
  if (!args[3]) args.set(3, py::None());
  if (!args[4]) args.set(4, py::None());
  return obj(py::fallback_makedatatable).call(args);
}



}  // namespace py
