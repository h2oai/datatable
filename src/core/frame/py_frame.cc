//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#include "ltype.h"
#include "python/_all.h"
#include "python/string.h"
#include "stype.h"
namespace py {

PyObject* Frame_Type = nullptr;



//------------------------------------------------------------------------------
// head()
//------------------------------------------------------------------------------

static const char* doc_head =
R"(head(self, n=10)
--

Return the first `n` rows of the frame.

If the number of rows in the frame is less than `n`, then all rows
are returned.

This is a convenience function and it is equivalent to `DT[:n, :]`.

Parameters
----------
n : int
    The maximum number of rows to return, 10 by default. This number
    cannot be negative.

return: Frame
    A frame containing the first up to `n` rows from the original
    frame, and same columns.


Examples
--------

>>> DT = dt.Frame(A=["apples", "bananas", "cherries", "dates",
...                  "eggplants", "figs", "grapes", "kiwi"])
>>> DT.head(4)
   | A
   | <str32>
-- + --------
 0 | apples
 1 | bananas
 2 | cherries
 3 | dates
--
[4 rows x 1 column]


See also
--------
- :meth:`.tail` -- return the last `n` rows of the Frame.
)";

static PKArgs args_head(
    1, 0, 0, false, false, {"n"}, "head", doc_head);


oobj Frame::head(const PKArgs& args) {
  size_t n = std::min(args.get<size_t>(0, 10),
                      dt->nrows());
  return m__getitem__(otuple(oslice(0, static_cast<int64_t>(n), 1),
                             None()));
}




//------------------------------------------------------------------------------
// tail()
//------------------------------------------------------------------------------

static const char* doc_tail =
R"(tail(self, n=10)
--

Return the last `n` rows of the frame.

If the number of rows in the frame is less than `n`, then all rows
are returned.

This is a convenience function and it is equivalent to `DT[-n:, :]`
(except when `n` is 0).

Parameters
----------
n : int
    The maximum number of rows to return, 10 by default. This number
    cannot be negative.

return: Frame
    A frame containing the last up to `n` rows from the original
    frame, and same columns.


Examples
--------

>>> DT = dt.Frame(A=["apples", "bananas", "cherries", "dates",
...                  "eggplants", "figs", "grapes", "kiwi"])
>>> DT.tail(3)
   | A
   | <str32>
-- + -------
 0 | figs
 1 | grapes
 2 | kiwi
--
[3 rows x 1 column]


See also
--------
- :meth:`.head` -- return the first `n` rows of the Frame.
)";

static PKArgs args_tail(
    1, 0, 0, false, false, {"n"}, "tail", doc_tail);


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

static const char* doc_copy =
R"(copy(self, deep=False)
--

Make a copy of the frame.

The returned frame will be an identical copy of the original,
including column names, types, and keys.

By default, copying is shallow with copy-on-write semantics. This
means that only the minimal information about the frame is copied,
while all the internal data buffers are shared between the copies.
Nevertheless, due to the copy-on-write semantics, any changes made to
one of the frames will not propagate to the other; instead, the data
will be copied whenever the user attempts to modify it.

It is also possible to explicitly request a deep copy of the frame
by setting the parameter `deep` to `True`. With this flag, the
returned copy will be truly independent from the original. The
returned frame will also be fully materialized in this case.


Parameters
----------
deep: bool
    Flag indicating whether to return a "shallow" (default), or a
    "deep" copy of the original frame.

(return): Frame
    A new Frame, which is the copy of the current frame.


Examples
--------

>>> DT1 = dt.Frame(range(5))
>>> DT2 = DT1.copy()
>>> DT2[0, 0] = -1
>>> DT2.to_list()
[[-1, 1, 2, 3, 4]]
>>> DT1.to_list()
[[0, 1, 2, 3, 4]]


Notes
-----
- Non-deep frame copy is a very low-cost operation: its speed depends
  on the number of columns only, not on the number of rows. On a
  regular laptop copying a 100-column frame takes about 30-50µs.

- Deep copying is more expensive, since the data has to be physically
  written to new memory, and if the source columns are virtual, then
  they need to be computed too.

- Another way to create a copy of the frame is using a `DT[i, j]`
  expression (however, this will not copy the key property)::

    DT[:, :]

- `Frame` class also supports copying via the standard Python library
  ``copy``::

    import copy
    DT_shallow_copy = copy.copy(DT)
    DT_deep_copy = copy.deepcopy(DT)

)";

static PKArgs args_copy(0, 0, 1, false, false, {"deep"}, "copy", doc_copy);


oobj Frame::copy(const PKArgs& args) {
  bool deepcopy = args[0].to<bool>(false);

  oobj res = Frame::oframe(deepcopy? new DataTable(*dt, DataTable::deep_copy)
                                   : new DataTable(*dt));
  Frame* newframe = static_cast<Frame*>(res.to_borrowed_ref());
  newframe->stypes = stypes;  Py_XINCREF(stypes);
  newframe->ltypes = ltypes;  Py_XINCREF(ltypes);
  newframe->source_ = source_;
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
// __len__()
//------------------------------------------------------------------------------

size_t Frame::m__len__() const {
  return dt->ncols();
}




//------------------------------------------------------------------------------
// export_names()
//------------------------------------------------------------------------------

static const char* doc_export_names =
R"(export_names(self)
--

.. xversionadded:: v0.10.0

Return a tuple of :ref:`f-expressions` for all columns of the frame.

For example, if the frame has columns "A", "B", and "C", then this
method will return a tuple of expressions ``(f.A, f.B, f.C)``. If you
assign these to, say, variables ``A``, ``B``, and ``C``, then you
will be able to write column expressions using the column names
directly, without using the ``f`` symbol::

    A, B, C = DT.export_names()
    DT[A + B > C, :]

The variables that are "exported" refer to each column *by name*. This
means that you can use the variables even after reordering the
columns. In addition, the variables will work not only for the frame
they were exported from, but also for any other frame that has columns
with the same names.

Parameters
----------
(return): Tuple[Expr, ...]
    The length of the tuple is equal to the number of columns in the
    frame. Each element of the tuple is a datatable *expression*, and
    can be used primarily with the ``DT[i,j]`` notation.

Notes
-----
- This method is effectively equivalent to::

    def export_names(self):
        return tuple(f[name] for name in self.names)

- If you want to export only a subset of column names, then you can
  either subset the frame first, or use ``*``-notation to ignore the
  names that you do not plan to use::

    A, B = DT[:, :2].export_names()  # export the first two columns
    A, B, *_ = DT.export_names()     # same

- Variables that you use in code do not have to have the same names
  as the columns::

    Price, Quantity = DT[:, ["sale price", "quant"]].export_names()

)";

static PKArgs args_export_names(
  0, 0, 0, false, false, {}, "export_names", doc_export_names);

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
  source_ = nullptr;
}


void Frame::_clear_types() {
  Py_XDECREF(stypes);
  Py_XDECREF(ltypes);
  stypes = nullptr;
  ltypes = nullptr;
  source_ = nullptr;
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
// .ncols
//------------------------------------------------------------------------------

static GSArgs args_ncols(
  "ncols",
  "Number of columns in the Frame\n");

oobj Frame::get_ncols() const {
  return py::oint(dt->ncols());
}



//------------------------------------------------------------------------------
// .nrows
//------------------------------------------------------------------------------

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



//------------------------------------------------------------------------------
// .shape
//------------------------------------------------------------------------------

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



//------------------------------------------------------------------------------
// .source
//------------------------------------------------------------------------------

static const char* doc_source =
R"(
The name of the file where this frame was loaded from.

This is a read-only property that describes the origin of the frame.
When a frame is loaded from a Jay or CSV file, this property will
contain the name of that file. Similarly, if the frame was opened
from a URL or a from a shell command, the source will report the
original URL / the command.

Certain sources may be converted into a Frame only partially,
in such case the ``source`` property will attempt to reflect this
fact. For example, when opening a multi-file zip archive, the
source will contain the name of the file within the archive.
Similarly, when opening an XLS file with several worksheets, the
source property will contain the name of the XLS file, the name of
the worksheet, and possibly even the range of cells that were read.

Parameters
----------
(return): str | None
    If the frame was loaded from a file or similar resource, the
    name of that file is returned. If the frame was computed, or its
    data modified, the property will return ``None``.
)";

static GSArgs args_source("source", doc_source);

oobj Frame::get_source() const {
  return source_? source_ : py::None();
}


void Frame::set_source(const std::string& src) {
  source_ = src.empty()? py::None() : py::ostring(src);
}



//------------------------------------------------------------------------------
// .stypes
//------------------------------------------------------------------------------

static GSArgs args_stypes(
  "stypes",
  "The tuple of each column's stypes (\"storage types\")\n");

oobj Frame::get_stypes() const {
  if (stypes == nullptr) {
    py::otuple ostypes(dt->ncols());
    for (size_t i = 0; i < ostypes.size(); ++i) {
      dt::SType st = dt->get_column(i).stype();
      ostypes.set(i, dt::stype_to_pyobj(st));
    }
    stypes = std::move(ostypes).release();
  }
  return oobj(stypes);
}



//------------------------------------------------------------------------------
// .stype
//------------------------------------------------------------------------------

static GSArgs args_stype(
  "stype",
  "The common stype for all columns.\n\n"
  "This property is well-defined only for frames where all columns\n"
  "share the same stype. For heterogeneous frames accessing this\n"
  "property will raise an error. For 0-column frames this property\n"
  "returns None.\n");

oobj Frame::get_stype() const {
  if (dt->ncols() == 0) return None();
  dt::SType stype = dt->get_column(0).stype();
  for (size_t i = 1; i < dt->ncols(); ++i) {
    dt::SType col_stype = dt->get_column(i).stype();
    if (col_stype != stype) {
      throw ValueError() << "The stype of column '" << dt->get_names()[i]
          << "' is `" << col_stype << "`, which is different from the "
          "stype of the previous column" << (i>1? "s" : "");
    }
  }
  return stype_to_pyobj(stype);
}



//------------------------------------------------------------------------------
// .ltypes
//------------------------------------------------------------------------------

static GSArgs args_ltypes(
  "ltypes",
  "The tuple of each column's ltypes (\"logical types\")\n");

oobj Frame::get_ltypes() const {
  if (ltypes == nullptr) {
    py::otuple oltypes(dt->ncols());
    for (size_t i = 0; i < oltypes.size(); ++i) {
      dt::SType st = dt->get_column(i).stype();
      oltypes.set(i, dt::ltype_to_pyobj(stype_to_ltype(st)));
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
  xt.add(METHOD__LEN__(&Frame::m__len__));
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
  _init_newsort(xt);
  _init_tocsv(xt);
  _init_tonumpy(xt);
  _init_topython(xt);

  xt.add(GETTER(&Frame::get_ltypes, args_ltypes));
  xt.add(GETTER(&Frame::get_ncols, args_ncols));
  xt.add(GETTER(&Frame::get_ndims, args_ndims));
  xt.add(GETSET(&Frame::get_nrows, &Frame::set_nrows, args_nrows));
  xt.add(GETTER(&Frame::get_shape, args_shape));
  xt.add(GETTER(&Frame::get_source, args_source));
  xt.add(GETTER(&Frame::get_stype,  args_stype));
  xt.add(GETTER(&Frame::get_stypes, args_stypes));

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
