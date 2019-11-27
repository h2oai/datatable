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
#include "python/string.h"
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
                      dt->nrows());
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
                      dt->nrows());
  // Note: usual slice `-n::` doesn't work as expected when `n = 0`
  int64_t start = static_cast<int64_t>(dt->nrows() - n);
  return m__getitem__(otuple(oslice(start, oslice::NA, 1),
                             None()));
}



//------------------------------------------------------------------------------
// copy()
//------------------------------------------------------------------------------

static PKArgs args_copy(
  0, 0, 1, false, false,
  {"deep"}, "copy",

R"(copy(self, deep=False)
--

Make a copy of this frame.

By default, this method creates a shallow copy of the current frame:
only references are copied, not the data itself. However, due to
copy-on-write semantics any changes made to one of the frames will not
propagate to the other. Thus, for most intents and purposes the copied
frame will behave as if it was deep-copied.

Still, it is possible to explicitly request a deep copy of the frame,
using the parameter `deep=True`. Even though it is not needed most of
the time, there still could be situations where you may want to use
this parameter: for example for auditing purposes, or if you want to
explicitly control the moment when the copying is made.
)");

oobj Frame::copy(const PKArgs& args) {
  bool deepcopy = args[0].to<bool>(false);

  oobj res = Frame::oframe(deepcopy? new DataTable(*dt, DataTable::deep_copy)
                                   : new DataTable(*dt));
  Frame* newframe = static_cast<Frame*>(res.to_borrowed_ref());
  newframe->stypes = stypes;  Py_XINCREF(stypes);
  newframe->ltypes = ltypes;  Py_XINCREF(ltypes);
  return res;
}


oobj Frame::m__copy__() {
  args_copy.bind(nullptr, nullptr);
  return copy(args_copy);
}


static PKArgs args___deepcopy__(
  0, 1, 0, false, false, {"memo"}, "__deepcopy__", nullptr);

oobj Frame::m__deepcopy__(const PKArgs&) {
  py::odict dict_arg;
  dict_arg.set(py::ostring("deep"), py::True());
  args_copy.bind(nullptr, dict_arg.to_borrowed_ref());
  return copy(args_copy);
}




//------------------------------------------------------------------------------
// export_names()
//------------------------------------------------------------------------------

static PKArgs args_export_names(
  0, 0, 0, false, false,
  {}, "export_names",

R"(export_names(self)
--

Return f-variables for each column of this frame.

For example, if the frame has columns A, B, and C, then this method
will return a tuple of expressions ``(f.A, f.B, f.C)``. If you assign
these expressions to variables A, B, and C, then you will be able to
write column expressions using the column names directly, without
using the f symbol::

    A, B, C = DT.export_names()
    DT[A + B > C, :]

This method is effectively equivalent to::

    return tuple(f[name] for name in self.names)

)");

oobj Frame::export_names(const PKArgs&) {
  py::oobj f = py::oobj::import("datatable", "f");
  py::otuple names = dt->get_pynames();
  py::otuple out_vars(names.size());
  for (size_t i = 0; i < dt->ncols(); ++i) {
    out_vars.set(i, f.get_item(names[i]));
  }
  return std::move(out_vars);
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

oobj Frame::oframe(DataTable&& dt) {
  return oframe(new DataTable(std::move(dt)));
}


oobj Frame::oframe(robj src) {
  return robj(Frame_Type).call(otuple{src});
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




//------------------------------------------------------------------------------
// Materialize
//------------------------------------------------------------------------------

static const char* doc_materialize =
R"(materialize(self, to_memory=False)
--

Force all data in the Frame to be laid out physically.

In datatable, a Frame may contain "virtual" columns, i.e. columns
whose data is computed on-the-fly. This allows us to have better
performance for certain types of computations, while also reduce
the total memory footprint. The use of virtual columns is generally
transparent to the user, and datatable will materialize them as
needed.

However there could be situations where you might want to materialize
your Frame explicitly. In particular, materialization will carry out
all delayed computations and break internal references on other
Frames' data. Thus, for example if you subset a large frame to create
a smaller subset, then the new frame will carry an internal reference
to the original, preventing it from being garbage-collected. However,
if you materialize the small frame, then the data will be physically
copied, allowing the original frame's memory to be freed.

Parameters
----------
to_memory: bool
    If True, then, in addition to de-virtualizing all columns, this
    method will also copy all memory-mapped columns into the RAM.

    When you open a Jay file, the Frame that is created will contain
    memory-mapped columns whose data still resides on disk. Calling
    ``.materialize(to_memory=True)`` will force the data to be loaded
    into the main memory. This may be beneficial if you are concerned
    about the disk speed, or if the file is on a removable drive, or
    if you want to delete the source file.

Returns
-------
None, this operation applies to the Frame and modifies it in-place.
)";

static PKArgs args_materialize(
  0, 1, 0, false, false, {"to_memory"}, "materialize", doc_materialize);

void Frame::materialize(const PKArgs& args) {
  bool to_memory = args[0].to<bool>(false);
  dt->materialize(to_memory);
}




//------------------------------------------------------------------------------
// Getters / setters
//------------------------------------------------------------------------------

static GSArgs args_ncols(
  "ncols",
  "Number of columns in the Frame\n");

oobj Frame::get_ncols() const {
  return py::oint(dt->ncols());
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
  return py::oint(dt->nrows());
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
    py::otuple ostypes(dt->ncols());
    for (size_t i = 0; i < ostypes.size(); ++i) {
      SType st = dt->get_column(i).stype();
      ostypes.set(i, info(st).py_stype());
    }
    stypes = std::move(ostypes).release();
  }
  return oobj(stypes);
}


static GSArgs args_stype(
  "stype",
  "The common stype for all columns.\n\n"
  "This property is well-defined only for frames where all columns\n"
  "share the same stype. For heterogeneous frames accessing this\n"
  "property will raise an error. For 0-column frames this property\n"
  "returns None.\n");

oobj Frame::get_stype() const {
  if (dt->ncols() == 0) return None();
  SType stype = dt->get_column(0).stype();
  for (size_t i = 1; i < dt->ncols(); ++i) {
    SType col_stype = dt->get_column(i).stype();
    if (col_stype != stype) {
      throw ValueError() << "The stype of column '" << dt->get_names()[i]
          << "' is `" << col_stype << "`, which is different from the "
          "stype of the previous column" << (i>1? "s" : "");
    }
  }
  return info(stype).py_stype();
}



static GSArgs args_ltypes(
  "ltypes",
  "The tuple of each column's ltypes (\"logical types\")\n");

oobj Frame::get_ltypes() const {
  if (ltypes == nullptr) {
    py::otuple oltypes(dt->ncols());
    for (size_t i = 0; i < oltypes.size(); ++i) {
      SType st = dt->get_column(i).stype();
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
  xt.set_class_name("datatable.Frame");
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
  Frame_Type = reinterpret_cast<PyObject*>(&Frame::type);

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
  xt.add(GETTER(&Frame::get_stype,  args_stype));
  xt.add(GETTER(&Frame::get_ltypes, args_ltypes));
  xt.add(GETTER(&Frame::get_ndims, args_ndims));

  xt.add(METHOD(&Frame::head, args_head));
  xt.add(METHOD(&Frame::tail, args_tail));
  xt.add(METHOD(&Frame::copy, args_copy));
  xt.add(METHOD(&Frame::materialize, args_materialize));
  xt.add(METHOD(&Frame::export_names, args_export_names));
  xt.add(METHOD0(&Frame::get_names, "keys"));
  xt.add(METHOD0(&Frame::m__copy__, "__copy__"));
  xt.add(METHOD(&Frame::m__deepcopy__, args___deepcopy__));
}




}  // namespace py
