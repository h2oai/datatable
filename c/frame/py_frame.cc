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
  return "datatable.core.Frame";
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

void Frame::Type::init_getsetters(GetSetters& gs)
{
  gs.add<&Frame::get_ncols>("ncols",
    "Number of columns in the Frame\n");
  gs.add<&Frame::get_nrows, &Frame::set_nrows>("nrows",
    "Number of rows in the Frame.\n"
    "\n"
    "Assigning to this property will change the height of the Frame,\n"
    "either by truncating if the new number of rows is smaller than the\n"
    "current, or filling with NAs if the new number of rows is greater.\n"
    "\n"
    "Increasing the number of rows of a keyed Frame is not allowed.\n");
  gs.add<&Frame::get_shape>("shape",
    "Tuple with (nrows, ncols) dimensions of the Frame\n");
  gs.add<&Frame::get_stypes>("stypes",
    "The tuple of stypes (\"storage type\") for each column\n");
  gs.add<&Frame::get_key>("key");
  gs.add<&Frame::get_internal>("internal", "[DEPRECATED]");
  gs.add<&Frame::get_internal, &Frame::set_internal>("_dt");
}

void Frame::Type::init_methods(Methods&) {
  // mm.add<&Frame::bang, args_bang>("bang");
}



void Frame::m__dealloc__() {
  Py_XDECREF(core_dt);
  Py_XDECREF(stypes);
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

void Frame::set_nrows(obj nr) {
  if (!nr.is_int()) {
    throw TypeError() << "Number of rows must be an integer, not "
        << nr.typeobj();
  }
  int64_t new_nrows = nr.to_int64_strict();
  if (new_nrows < 0) {
    throw ValueError() << "Number of rows cannot be negative";
  }
  dt->resize_rows(new_nrows);
}


oobj Frame::get_shape() const {
  py::otuple shape(2);
  shape.set(0, get_nrows());
  shape.set(1, get_ncols());
  return shape;
}


oobj Frame::get_stypes() const {
  if (stypes == nullptr) {
    py::otuple ostypes(dt->ncols);
    for (size_t i = 0; i < ostypes.size(); ++i) {
      SType st = dt->columns[i]->stype();
      ostypes.set(i, info(st).py_stype());
    }
    stypes = ostypes.release();
  }
  return oobj(stypes);
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
  core_dt->_frame = this;
}


}  // namespace py
