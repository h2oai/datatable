//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "frame/py_frame.h"
#include <iostream>
#include "python/int.h"

namespace py {


//------------------------------------------------------------------------------
// Declare Frame's API
//------------------------------------------------------------------------------

NoArgs Frame::Type::args_bang;
PKArgs Frame::Type::args___init__(1, 0, 3, false, false,
                                  {"src", "names", "stypes", "stype"});


const char* Frame::Type::classname() {
  return "datatable.Frame";
}

const char* Frame::Type::classdoc() {
  return
    "Two-dimensional column-oriented table of data. Each column has its own\n"
    "name and type. Types may vary across columns but cannot vary within\n"
    "each column.\n"
    "\n"
    "Internally the data is stored as C primitives, and processed using\n"
    "multithreaded native C++ code.\n"
    "\n"
    "This is a primary data structure for datatable module.\n";
}

void Frame::Type::init_getsetters(GetSetters& gs) {
  gs.add<&Frame::get_ncols>("ncols");
  gs.add<&Frame::get_nrows>("nrows");
}

void Frame::Type::init_methods(Methods& mm) {
  mm.add<&Frame::bang, args_bang>("bang");
}



void Frame::m__dealloc__() {
  std::cout << "In Frame::dealloc (dt=" << dt << ")\n";
}

void Frame::m__get_buffer__(Py_buffer* , int ) const {
}

void Frame::m__release_buffer__(Py_buffer*) const {
}

oobj Frame::get_ncols() const {
  return oobj(py::Int(11));
}

oobj Frame::get_nrows() const {
  return oobj(py::Int(47));
}

oobj Frame::bang(NoArgs&) {
  std::cout << "Yay, Frame::bang()!\n";
  return None();
}


}  // namespace py
