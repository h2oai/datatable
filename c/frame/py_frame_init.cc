//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "frame/py_frame.h"
#include <iostream>
#include <string>
#include <vector>

namespace py {


//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

static std::vector<std::string> _get_names(const Arg& arg) {
  if (arg.is_undefined() || arg.is_none()) {
    return std::vector<std::string>();
  }
  // if (arg.is_list() || arg.is_tuple()) {
  //   return arg.to_list_of_strs();
  // }
  // TODO: also allow a single-column Frame of string type
  throw TypeError() << arg.name() << " must be a list/tuple of column names";
}


static DataTable* _make_empty_frame() {
  Column** cols = new Column*[1];
  cols[0] = nullptr;
  return new DataTable(cols);
}


static DataTable* _make_frame_from_list(py::list list) {
  if (list.size() == 0) {
    return _make_empty_frame();
  }
  return nullptr;
}



//------------------------------------------------------------------------------
// Main constructor
//------------------------------------------------------------------------------

void Frame::m__init__(PKArgs& args) {
  const Arg& src        = args[0];
  const Arg& names_arg  = args[1];
  const Arg& stypes_arg = args[2];

  // bool names_defined = !names_arg.is_undefined();
  auto names = _get_names(names_arg);

  if (src.is_list_or_tuple()) {
    dt = _make_frame_from_list(src.to_pylist());
  }
  else if (src.is_undefined() || src.is_none()) {
    dt = _make_empty_frame();
  }
  else {
    std::cout << "Creating a frame from ";
    src.print();
  }
}


}  // namespace py
