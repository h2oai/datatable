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
  return m__getitem__(otuple(oslice(0, static_cast<int64_t>(n), 1),
                             None()));
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
  int64_t start = static_cast<int64_t>(dt->nrows - n);
  return m__getitem__(otuple(oslice(start, oslice::NA, 1),
                             None()));
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
  oobj res = Frame::oframe(dt->copy());
  Frame* newframe = static_cast<Frame*>(res.to_borrowed_ref());
  newframe->stypes = stypes;  Py_XINCREF(stypes);
  newframe->ltypes = ltypes;  Py_XINCREF(ltypes);
  return res;
}



//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------
bool Frame::internal_construction = false;


oobj Frame::oframe(DataTable* dt) {
  Frame::internal_construction = true;
  PyObject* res = PyObject_CallObject(Frame_Type, nullptr);
  Frame::internal_construction = false;
  if (!res) throw PyError();

  Frame* frame = reinterpret_cast<Frame*>(res);
  frame->dt = dt;
  return oobj::from_new_reference(frame);
}


void Frame::m__dealloc__() {
  Py_XDECREF(stypes);
  Py_XDECREF(ltypes);
  delete dt;
  dt = nullptr;
}


void Frame::_clear_types() const {
  Py_XDECREF(stypes);
  Py_XDECREF(ltypes);
  stypes = nullptr;
  ltypes = nullptr;
}


static PKArgs args_materialize(
  0, 0, 0, false, false, {}, "materialize",

R"(materialize(self)
--

Convert a "view" frame into a regular data frame.

Certain datatable operation produce frames that contain "view"
columns. These columns refer to the data in some other column, via
a RowIndex object that describes which values from the other column
should be picked. This is done in order to improve performance and
reduce memory usage of certain operations: a view column avoids
copying data from its parent column.

Usually view columns are created transparently to the user, and they
are materialized by datatable when necessary. This method, on the
other hand, will force all view columns in the frame to be
materialized immediately.
)");

void Frame::materialize(const PKArgs&) {
  dt->materialize();
}




//------------------------------------------------------------------------------
// Getters / setters
//------------------------------------------------------------------------------

static GSArgs args_ncols(
  "ncols",
  "Number of columns in the Frame\n");

oobj Frame::get_ncols() const {
  return py::oint(dt->ncols);
}


static GSArgs args_nrows(
  "nrows",
R"(Number of rows in the Frame.

Assigning to this property will change the height of the Frame,
either by truncating if the new number of rows is smaller than the
current, or filling with NAs if the new number of rows is greater.

Increasing the number of rows of a keyed Frame is not allowed.
)");

oobj Frame::get_nrows() const {
  return py::oint(dt->nrows);
}

void Frame::set_nrows(const Arg& nr) {
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


static GSArgs args_shape(
  "shape",
  "Tuple with (nrows, ncols) dimensions of the Frame\n");

oobj Frame::get_shape() const {
  return otuple(get_nrows(), get_ncols());
}


static GSArgs args_ndims(
  "ndims",
  "Number of dimensions in the Frame, always 2\n");

oobj Frame::get_ndims() const {
  return oint(2);
}


static GSArgs args_stypes(
  "stypes",
  "The tuple of each column's stypes (\"storage types\")\n");

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


static GSArgs args_ltypes(
  "ltypes",
  "The tuple of each column's ltypes (\"logical types\")\n");

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




//------------------------------------------------------------------------------
// Declare Frame's API
//------------------------------------------------------------------------------

static PKArgs args___init__(1, 0, 3, false, true,
                            {"src", "names", "stypes", "stype"},
                            "__init__", nullptr);

void Frame::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.core.Frame");
  xt.set_class_doc(
    "Two-dimensional column-oriented table of data. Each column has its own\n"
    "name and type. Types may vary across columns but cannot vary within\n"
    "each column.\n"
    "\n"
    "Internally the data is stored as C primitives, and processed using\n"
    "multithreaded native C++ code.\n"
    "\n"
    "This is a primary data structure for the `datatable` module.\n"
  );
  xt.set_subclassable(true);
  xt.add(CONSTRUCTOR(&Frame::m__init__, args___init__));
  xt.add(DESTRUCTOR(&Frame::m__dealloc__));
  xt.add(METHOD__GETITEM__(&Frame::m__getitem__));
  xt.add(METHOD__SETITEM__(&Frame::m__setitem__));
  xt.add(BUFFERS(&Frame::m__getbuffer__, &Frame::m__releasebuffer__));

  _init_cbind(xt);
  _init_key(xt);
  _init_init(xt);
  _init_iter(xt);
  _init_jay(xt);
  _init_names(xt);
  _init_rbind(xt);
  _init_replace(xt);
  _init_repr(xt);
  _init_sizeof(xt);
  _init_stats(xt);
  _init_sort(xt);
  _init_tocsv(xt);
  _init_tonumpy(xt);
  _init_topython(xt);

  xt.add(GETTER(&Frame::get_ncols, args_ncols));
  xt.add(GETSET(&Frame::get_nrows, &Frame::set_nrows, args_nrows));
  xt.add(GETTER(&Frame::get_shape, args_shape));
  xt.add(GETTER(&Frame::get_stypes, args_stypes));
  xt.add(GETTER(&Frame::get_ltypes, args_ltypes));
  xt.add(GETTER(&Frame::get_ndims, args_ndims));

  xt.add(METHOD(&Frame::head, args_head));
  xt.add(METHOD(&Frame::tail, args_tail));
  xt.add(METHOD(&Frame::copy, args_copy));
  xt.add(METHOD(&Frame::materialize, args_materialize));
  xt.add(METHOD0(&Frame::get_names, "keys"));
}




}  // namespace py
