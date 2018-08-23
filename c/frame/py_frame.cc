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
PKArgs Frame::Type::args_colindex(1, 0, 0, false, false, {"name"});


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
    "The tuple of each column's stypes (\"storage types\")\n");

  gs.add<&Frame::get_ltypes>("ltypes",
    "The tuple of each column's ltypes (\"logical types\")\n");

  gs.add<&Frame::get_names, &Frame::set_names>("names",
    "Tuple of column names.\n"
    "\n"
    "You can rename the Frame's columns by assigning a new list/tuple of\n"
    "names to this property. The length of the new list of names must be\n"
    "the same as the number of columns in the Frame.\n"
    "\n"
    "It is also possible to rename just a few columns by assigning a\n"
    "dictionary ``{oldname: newname, ...}``. Any column not listed in the\n"
    "dictionary will retain its name.\n"
    "\n"
    "Examples\n"
    "--------\n"
    ">>> d0 = dt.Frame([[1], [2], [3]])\n"
    ">>> d0.names = ['A', 'B', 'C']\n"
    ">>> d0.names\n"
    "('A', 'B', 'C')\n"
    ">>> d0.names = {'B': 'middle'}\n"
    ">>> d0.names\n"
    "('A', 'middle', 'C')\n"
    ">>> del d0.names\n"
    ">>> d0.names\n"
    "('C0', 'C1', 'C2')");

  gs.add<&Frame::get_key>("key");
  gs.add<&Frame::get_internal>("internal", "[DEPRECATED]");
  gs.add<&Frame::get_internal, &Frame::set_internal>("_dt");
}


void Frame::Type::init_methods(Methods& mm) {
  mm.add<&Frame::colindex, args_colindex>("colindex",
    "colindex(self, name)\n"
    "--\n\n"
    "Return index of the column ``name``.\n"
    "\n"
    ":param name: name of the column to find the index for. This can also\n"
    "    be an index of a column, in which case the index is checked that\n"
    "    it doesn't go out-of-bounds, and negative index is converted into\n"
    "    positive.\n"
    ":raises ValueError: if the requested column does not exist.\n");
}



//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

void Frame::m__dealloc__() {
  Py_XDECREF(core_dt);
  Py_XDECREF(stypes);
  Py_XDECREF(ltypes);
  Py_XDECREF(names);
  Py_XDECREF(inames);
  dt = nullptr;  // `dt` is already managed by `core_dt`
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


oobj Frame::get_ltypes() const {
  if (ltypes == nullptr) {
    py::otuple oltypes(dt->ncols);
    for (size_t i = 0; i < oltypes.size(); ++i) {
      SType st = dt->columns[i]->stype();
      oltypes.set(i, info(st).py_ltype());
    }
    ltypes = oltypes.release();
  }
  return oobj(ltypes);
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
  oobj tmp(_dt);  // In case the new _dt is same as, or related to the current
                  // Frame, we do no want it to be deleted. By creating a new
                  // reference to `_dt` we ensure it can't get deleted.
  m__dealloc__();
  dt = _dt.to_frame();
  core_dt = static_cast<pydatatable::obj*>(_dt.to_pyobject_newref());
  core_dt->_frame = this;
}


}  // namespace py
