//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#include <iostream>
#include "frame/py_frame.h"
#include "python/_all.h"

extern SType force_stype;

namespace py {

PyObject* Frame_Type = nullptr;



//------------------------------------------------------------------------------
// head() & tail()
//------------------------------------------------------------------------------

static PKArgs args_head(
    1, 0, 0, false, false,
    {"n"}, "head",
R"(head(self, n=10)
--

Return the first `n` rows of the frame, same as ``self[:n, :]``.
)");

oobj Frame::head(const PKArgs& args) {
  size_t n = std::min(args.get<size_t>(0, 10),
                      dt->nrows);
  py::otuple aa(2);
  aa.set(0, py::oslice(0, static_cast<int64_t>(n), 1));
  aa.set(1, py::None());
  return m__getitem__(aa);
}



static PKArgs args_tail(
    1, 0, 0, false, false,
    {"n"}, "tail",
R"(tail(self, n=10)
--

Return the last `n` rows of the frame, same as ``self[-n:, :]``.
)");

oobj Frame::tail(const PKArgs& args) {
  size_t n = std::min(args.get<size_t>(0, 10),
                      dt->nrows);
  // Note: usual slice `-n::` doesn't work as expected when `n = 0`
  py::otuple aa(2);
  aa.set(0, py::oslice(static_cast<int64_t>(dt->nrows - n), py::oslice::NA, 1));
  aa.set(1, py::None());
  return m__getitem__(aa);
}



//------------------------------------------------------------------------------
// copy()
//------------------------------------------------------------------------------

static PKArgs args_copy(
  0, 0, 0, false, false,
  {}, "copy",

R"(copy(self)
--

Make a copy of this frame.

This method creates a shallow copy of the current frame: only references
are copied, not the data itself. However, due to copy-on-write semantics
any changes made to one of the frames will not propagate to the other.
Thus, for all intents and purposes the copied frame will behave as if
it was deep-copied.)"
);

oobj Frame::copy(const PKArgs&) {
  Frame* newframe = Frame::from_datatable(dt->copy());
  newframe->stypes = stypes;  Py_XINCREF(stypes);
  newframe->ltypes = ltypes;  Py_XINCREF(ltypes);
  return py::oobj::from_new_reference(newframe);
}



//------------------------------------------------------------------------------
// Declare Frame's API
//------------------------------------------------------------------------------

PKArgs Frame::Type::args___init__(1, 0, 3, false, true,
                                  {"src", "names", "stypes", "stype"},
                                  "__init__", nullptr);


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


void Frame::Type::init_methods_and_getsets(Methods& mm, GetSetters& gs)
{
  _init_init(mm);
  _init_names(mm, gs);
  _init_cbind(mm);

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

  gs.add<&Frame::get_key, &Frame::set_key>("key",
    "Tuple of column names that serve as a primary key for this Frame.\n"
    "\n"
    "If the Frame is not keyed, this will return an empty tuple.\n"
    "\n"
    "Assigning to this property will make the Frame keyed by the specified\n"
    "column(s). The key columns will be moved to the front, and the Frame\n"
    "will be sorted. The values in the key columns must be unique.\n");

  gs.add<&Frame::get_internal>("internal", "[DEPRECATED]");
  gs.add<&Frame::get_internal>("_dt");

  mm.add<&Frame::replace, args_replace>();
  mm.add<&Frame::_repr_html_, args__repr_html_>();
  mm.add<&Frame::to_dict, args_to_dict>();
  mm.add<&Frame::to_list, args_to_list>();
  mm.add<&Frame::to_tuples, args_to_tuples>();
  mm.add<&Frame::to_numpy, fn_to_numpy>();

  ADD_METHOD(mm, &Frame::head, args_head);
  ADD_METHOD(mm, &Frame::tail, args_tail);
  ADD_METHOD(mm, &Frame::copy, args_copy);
}



//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------
bool Frame::internal_construction = false;


Frame* Frame::from_datatable(DataTable* dt) {
  // PyObject* pytype = reinterpret_cast<PyObject*>(&Frame::Type::type);
  Frame::internal_construction = true;
  PyObject* res = PyObject_CallObject(Frame_Type, nullptr);
  Frame::internal_construction = false;
  if (!res) throw PyError();

  PyObject* _dt = pydatatable::wrap(dt);
  if (!_dt) throw PyError();

  Frame* frame = reinterpret_cast<Frame*>(res);
  frame->dt = dt;
  frame->core_dt = reinterpret_cast<pydatatable::obj*>(_dt);
  frame->core_dt->_frame = frame;
  return frame;
}


void Frame::m__dealloc__() {
  Py_XDECREF(core_dt);
  Py_XDECREF(stypes);
  Py_XDECREF(ltypes);
  dt = nullptr;  // `dt` is already managed by `core_dt`
}

void Frame::m__get_buffer__(Py_buffer*, int) const {
}

void Frame::m__release_buffer__(Py_buffer*) const {
}


void Frame::_clear_types() const {
  Py_XDECREF(stypes);
  Py_XDECREF(ltypes);
  stypes = nullptr;
  ltypes = nullptr;
}




//------------------------------------------------------------------------------
// Getters / setters
//------------------------------------------------------------------------------

oobj Frame::get_ncols() const {
  return py::oint(dt->ncols);
}


oobj Frame::get_nrows() const {
  return py::oint(dt->nrows);
}

void Frame::set_nrows(py::robj nr) {
  if (!nr.is_int()) {
    throw TypeError() << "Number of rows must be an integer, not "
        << nr.typeobj();
  }
  int64_t new_nrows = nr.to_int64_strict();
  if (new_nrows < 0) {
    throw ValueError() << "Number of rows cannot be negative";
  }
  dt->resize_rows(static_cast<size_t>(new_nrows));
}


oobj Frame::get_shape() const {
  py::otuple shape(2);
  shape.set(0, get_nrows());
  shape.set(1, get_ncols());
  return std::move(shape);
}


oobj Frame::get_stypes() const {
  if (stypes == nullptr) {
    py::otuple ostypes(dt->ncols);
    for (size_t i = 0; i < ostypes.size(); ++i) {
      SType st = dt->columns[i]->stype();
      ostypes.set(i, info(st).py_stype());
    }
    stypes = std::move(ostypes).release();
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
    ltypes = std::move(oltypes).release();
  }
  return oobj(ltypes);
}


oobj Frame::get_internal() const {
  return oobj(core_dt);
}


}  // namespace py
