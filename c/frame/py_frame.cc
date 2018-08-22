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
#include "python/tuple.h"

namespace py {


//------------------------------------------------------------------------------
// Declare Frame's API
//------------------------------------------------------------------------------

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
    "This is a primary data structure for the `datatable` module.\n";
}

void Frame::Type::init_getsetters(GetSetters& gs) {
  gs.add<&Frame::get_ncols>("ncols");
  gs.add<&Frame::get_nrows>("nrows");
  gs.add<&Frame::get_key>("key");
  gs.add<&Frame::get_internal>("internal");
  gs.add<&Frame::get_internal, &Frame::set_internal>("_dt");
}

void Frame::Type::init_methods(Methods&) {
  // mm.add<&Frame::bang, args_bang>("bang");
}



void Frame::m__dealloc__() {
  Py_XDECREF(core_dt);
  // `dt` is already managed by `core_dt`.
  // delete dt;
  dt = nullptr;
}

void Frame::m__get_buffer__(Py_buffer* , int ) const {
}

void Frame::m__release_buffer__(Py_buffer*) const {
}


//------------------------------------------------------------------------------
// Getters / setters
//------------------------------------------------------------------------------

oobj Frame::get_ncols() const {
  return py::oInt(dt->ncols);
}

oobj Frame::get_nrows() const {
  return py::oInt(dt->nrows);
}

oobj Frame::get_key() const {
  py::otuple key(dt->nkeys);
  // Fill in the keys...
  return key;
}

oobj Frame::get_internal() const {
  return oobj(core_dt);
}

void Frame::set_internal(obj _dt) {
  m__dealloc__();
  dt = _dt.to_frame();
  core_dt = static_cast<pydatatable::obj*>(_dt.to_pyobject_newref());
}


}  // namespace py
