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
#include <algorithm>          // std::min
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
   | str32
-- + --------
 0 | apples
 1 | bananas
 2 | cherries
 3 | dates
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
   | str32
-- + ------
 0 | figs
 1 | grapes
 2 | kiwi
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

return: Frame
    A new Frame, which is the copy of the current frame.


Examples
--------

>>> DT1 = dt.Frame(range(5))
>>> DT2 = DT1.copy()
>>> DT2[0, 0] = -1
>>> DT2
   |    C0
   | int32
-- + -----
 0 |    -1
 1 |     1
 2 |     2
 3 |     3
 4 |     4
[5 rows x 1 column]
>>> DT1
   |    C0
   | int32
-- + -----
 0 |     0
 1 |     1
 2 |     2
 3 |     3
 4 |     4
[5 rows x 1 column]


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

    >>> DT[:, :]

- `Frame` class also supports copying via the standard Python library
  ``copy``::

    >>> import copy
    >>> DT_shallow_copy = copy.copy(DT)
    >>> DT_deep_copy = copy.deepcopy(DT)

)";

static PKArgs args_copy(0, 0, 1, false, false, {"deep"}, "copy", doc_copy);


oobj Frame::copy(const PKArgs& args) {
  bool deepcopy = args[0].to<bool>(false);

  oobj res = Frame::oframe(deepcopy? new DataTable(*dt, DataTable::deep_copy)
                                   : new DataTable(*dt));
  Frame* newframe = static_cast<Frame*>(res.to_borrowed_ref());
  newframe->stypes = stypes;  Py_XINCREF(stypes);
  newframe->ltypes = ltypes;  Py_XINCREF(ltypes);
  newframe->meta_ = meta_;
  newframe->source_ = source_;
  return res;
}



static PKArgs args___deepcopy__(
  0, 1, 0, false, false, {"memo"}, "__deepcopy__", nullptr);

oobj Frame::m__deepcopy__(const PKArgs&) {
  py::odict dict_arg;
  dict_arg.set(py::ostring("deep"), py::True());
  args_copy.bind(nullptr, dict_arg.to_borrowed_ref());
  return copy(args_copy);
}

oobj Frame::m__copy__() {
  args_copy.bind(nullptr, nullptr);
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
.. x-version-added:: 0.10

Return a tuple of :ref:`f-expressions` for all columns of the frame.

For example, if the frame has columns "A", "B", and "C", then this
method will return a tuple of expressions ``(f.A, f.B, f.C)``. If you
assign these to, say, variables ``A``, ``B``, and ``C``, then you
will be able to write column expressions using the column names
directly, without using the ``f`` symbol::

    >>> A, B, C = DT.export_names()
    >>> DT[A + B > C, :]

The variables that are "exported" refer to each column *by name*. This
means that you can use the variables even after reordering the
columns. In addition, the variables will work not only for the frame
they were exported from, but also for any other frame that has columns
with the same names.

Parameters
----------
return: Tuple[Expr, ...]
    The length of the tuple is equal to the number of columns in the
    frame. Each element of the tuple is a datatable *expression*, and
    can be used primarily with the ``DT[i,j]`` notation.

Notes
-----
- This method is effectively equivalent to::

    >>> def export_names(self):
    ...     return tuple(f[name] for name in self.names)

- If you want to export only a subset of column names, then you can
  either subset the frame first, or use ``*``-notation to ignore the
  names that you do not plan to use::

    >>> A, B = DT[:, :2].export_names()  # export the first two columns
    >>> A, B, *_ = DT.export_names()     # same

- Variables that you use in code do not have to have the same names
  as the columns::

    >>> Price, Quantity = DT[:, ["sale price", "quant"]].export_names()

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
performance for certain types of computations, while also reducing
the total memory footprint. The use of virtual columns is generally
transparent to the user, and datatable will materialize them as
needed.

However, there could be situations where you might want to materialize
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

return: None
    This operation modifies the frame in-place.
)";

static PKArgs args_materialize(
  0, 1, 0, false, false, {"to_memory"}, "materialize", doc_materialize);

void Frame::materialize(const PKArgs& args) {
  bool to_memory = args[0].to<bool>(false);
  dt->materialize(to_memory);
}


//------------------------------------------------------------------------------
// .meta
//------------------------------------------------------------------------------

static const char* doc_meta =
R"(
.. x-version-added:: 1.0

Frame's meta information.

This property contains meta information, if any, as set by datatable
functions and methods. It is a settable property, so that users can also
update it with any information relevant to a particular frame.

It is not guaranteed that the existing meta information will be preserved
by the functions and methods called on the frame. In particular,
it is not preserved when exporting data into a Jay file or pickling the data.
This behavior may change in the future.

The default value for this property is `None`.


Parameters
----------
return: dict | None
    If the frame carries any meta information, the corresponding meta
    information dictionary is returned, `None` is returned otherwise.

new_meta: dict | None
    New meta information.

)";

static GSArgs args_meta("meta", doc_meta);

oobj Frame::get_meta() const {
  return meta_? meta_ : py::None();
}


void Frame::set_meta(const Arg& meta) {
  if (!meta.is_dict() && !meta.is_none()) {
    throw TypeError() << "`.meta` must be a dictionary or `None`, "
      << "instead got: " << meta.typeobj();
  }
  meta_ = meta.is_none()? py::None() : meta.to_pydict();
}



//------------------------------------------------------------------------------
// .ncols
//------------------------------------------------------------------------------

static const char* doc_ncols =
R"(
Number of columns in the frame.

Parameters
----------
return: int
    The number of columns can be either zero or a positive integer.

Notes
-----
The expression `len(DT)` also returns the number of columns in the
frame `DT`. Such usage, however, is not recommended.

See also
--------
- :attr:`.nrows`: getter for the number of rows of the frame.
)";

static GSArgs args_ncols("ncols", doc_ncols);

oobj Frame::get_ncols() const {
  return py::oint(dt->ncols());
}



//------------------------------------------------------------------------------
// .nrows
//------------------------------------------------------------------------------

static const char* doc_nrows =
R"(
Number of rows in the Frame.

Assigning to this property will change the height of the Frame,
either by truncating if the new number of rows is smaller than the
current, or filling with NAs if the new number of rows is greater.

Increasing the number of rows of a keyed Frame is not allowed.

Parameters
----------
return: int
    The number of rows can be either zero or a positive integer.

n: int
    The new number of rows for the frame, this should be a non-negative
    integer.

See also
--------
- :attr:`.ncols`: getter for the number of columns of the frame.
)";

static GSArgs args_nrows("nrows", doc_nrows);

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

static const char* doc_shape =
R"(
Tuple with ``(nrows, ncols)`` dimensions of the frame.

This property is read-only.

Parameters
----------
return: Tuple[int, int]
    Tuple with two integers: the first is the number of rows, the
    second is the number of columns.

See also
--------
- :attr:`.nrows` -- getter for the number of rows;
- :attr:`.ncols` -- getter for the number of columns.
)";

static GSArgs args_shape("shape", doc_shape);

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
.. x-version-added:: 0.11

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
return: str | None
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

static const char* doc_stypes =
R"(
The tuple of each column's stypes ("storage types").

Parameters
----------
return: Tuple[stype, ...]
    The length of the tuple is the same as the number of columns in
    the frame.

See also
--------
- :attr:`.stype` -- common stype for all columns
- :attr:`.ltypes` -- tuple of columns' logical types
)";

static GSArgs args_stypes("stypes", doc_stypes);

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

static const char* doc_stype =
R"(
.. x-version-added:: v0.10.0

The common :class:`dt.stype` for all columns.

This property is well-defined only for frames where all columns have
the same stype.

Parameters
----------
return: stype | None
    For frames where all columns have the same stype, this common
    stype is returned. If a frame has 0 columns, `None` will be
    returned.

except: InvalidOperationError
    This exception will be raised if the columns in the frame have
    different stypes.

See also
--------
- :attr:`.stypes` -- tuple of stypes for all columns.
)";

static GSArgs args_stype("stype", doc_stype);

oobj Frame::get_stype() const {
  if (dt->ncols() == 0) return None();
  dt::SType stype = dt->get_column(0).stype();
  for (size_t i = 1; i < dt->ncols(); ++i) {
    dt::SType col_stype = dt->get_column(i).stype();
    if (col_stype != stype) {
      throw InvalidOperationError()
          << "The stype of column '" << dt->get_names()[i]
          << "' is `" << col_stype << "`, which is different from the "
          "stype of the previous column" << (i>1? "s" : "");
    }
  }
  return stype_to_pyobj(stype);
}



//------------------------------------------------------------------------------
// .ltypes
//------------------------------------------------------------------------------

static const char* doc_ltypes =
R"(
The tuple of each column's ltypes ("logical types").

Parameters
----------
return: Tuple[ltype, ...]
    The length of the tuple is the same as the number of columns in
    the frame.

See also
--------
- :attr:`.stypes` -- tuple of columns' storage types
)";

static GSArgs args_ltypes("ltypes", doc_ltypes);

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

static const char* doc___init__ =
R"(__init__(self, _data=None, *, names=None, stypes=None, stype=None, **cols)
--

Create a new Frame from a single or multiple sources.

Argument `_data` (or `**cols`) contains the source data for Frame's
columns. Column names are either derived from the data, given
explicitly via the `names` argument, or generated automatically.
Either way, the constructor ensures that column names are unique,
non-empty, and do not contain certain special characters (see
:ref:`name-mangling` for details).

Parameters
----------
_data: Any
    The first argument to the constructor represents the source from
    which to construct the Frame. If this argument is given, then the
    varkwd arguments `**cols` should not be used.

    This argument can accept a wide range of data types; see the
    "Details" section below.

**cols: Any
    Sequence of varkwd column initializers. The keys become column
    names, and the values contain column data. Using varkwd arguments
    is equivalent to passing a `dict` as the `_data` argument.

    When varkwd initializers are used, the `names` parameter may not
    be given.

names: List[str|None]
    Explicit list (or tuple) of column names. The number of elements
    in the list must be the same as the number of columns being
    constructed.

    This parameter should not be used when constructing the frame
    from `**cols`.

stypes: List[stype-like] | Dict[str, stype-like]
    Explicit list (or tuple) of column types. The number of elements
    in the list must be the same as the number of columns being
    constructed.

stype: stype | type
    Similar to `stypes`, but provide a single type that will be used
    for all columns. This option cannot be specified together with
    `stypes`.

return: Frame
    A :class:`Frame <datatable.Frame>` object is constructed and
    returned.

except: ValueError
    The exception is raised if the lengths of `names` or `stypes`
    lists are different from the number of columns created, or when
    creating several columns and they have incompatible lengths.


Details
-------
The shape of the constructed Frame depends on the type of the source
argument `_data` (or `**cols`). The argument `_data` and varkwd
arguments `**cols` are mutually exclusive: they cannot be used at the
same time. However, it is possible to use neither and construct an
empty frame::

    >>> dt.Frame()       # empty 0x0 frame
    >>> dt.Frame(None)   # same
    >>> dt.Frame([])     # same

The varkwd arguments `**cols` can be used to construct a Frame by
columns. In this case the keys become column names, and the values
are column initializers. This form is mostly used for convenience;
it is equivalent to converting `cols` into a `dict` and passing as
the first argument::

    >>> dt.Frame(A = range(7),
    ...          B = [0.1, 0.3, 0.5, 0.7, None, 1.0, 1.5],
    ...          C = ["red", "orange", "yellow", "green", "blue", "indigo", "violet"])
    >>> # equivalent to
    >>> dt.Frame({"A": range(7),
    ...           "B": [0.1, 0.3, 0.5, 0.7, None, 1.0, 1.5],
    ...           "C": ["red", "orange", "yellow", "green", "blue", "indigo", "violet"]})
       |     A        B  C
       | int32  float64  str32
    -- + -----  -------  ------
     0 |     0      0.1  red
     1 |     1      0.3  orange
     2 |     2      0.5  yellow
     3 |     3      0.7  green
     4 |     4     NA    blue
     5 |     5      1    indigo
     6 |     6      1.5  violet
    [7 rows x 3 columns]

The argument `_data` accepts a wide range of input types. The
following list describes possible choices:

`List[List | Frame | np.array | pd.DataFrame | pd.Series | range | typed_list]`
    When the source is a non-empty list containing other lists or
    compound objects, then each item will be interpreted as a column
    initializer, and the resulting frame will have as many columns
    as the number of items in the list.

    Each element in the list must produce a single column. Thus,
    it is not allowed to use multi-column `Frame`s, or
    multi-dimensional numpy arrays or pandas `DataFrame`s.

        >>> dt.Frame([[1, 3, 5, 7, 11],
        ...           [12.5, None, -1.1, 3.4, 9.17]])
           |    C0       C1
           | int32  float64
        -- + -----  -------
         0 |     1    12.5
         1 |     3    NA
         2 |     5    -1.1
         3 |     7     3.4
         4 |    11     9.17
        [5 rows x 2 columns]

    Note that unlike `pandas` and `numpy`, we treat a list of lists
    as a list of columns, not a list of rows. If you need to create
    a Frame from a row-oriented store of data, you can use a list of
    dictionaries or a list of tuples as described below.

`List[Dict]`
    If the source is a list of `dict` objects, then each element
    in this list is interpreted as a single row. The keys
    in each dictionary are column names, and the values contain
    contents of each individual cell.

    The rows don't have to have the same number or order of
    entries: all missing elements will be filled with NAs::

        >>> dt.Frame([{"A": 3, "B": 7},
        ...           {"A": 0, "B": 11, "C": -1},
        ...           {"C": 5}])
           |     A      B      C
           | int32  int32  int32
        -- + -----  -----  -----
         0 |     3      7     NA
         1 |     0     11     -1
         2 |    NA     NA      5
        [3 rows x 3 columns]

    If the `names` parameter is given, then only the keys given
    in the list of names will be taken into account, all extra
    fields will be discarded.

`List[Tuple]`
    If the source is a list of `tuple`s, then each tuple
    represents a single row. The tuples must have the same size,
    otherwise an exception will be raised::

        >>> dt.Frame([(39, "Mary"),
        ...           (17, "Jasmine"),
        ...           (23, "Lily")], names=['age', 'name'])
           |   age  name
           | int32  str32
        -- + -----  -------
         0 |    39  Mary
         1 |    17  Jasmine
         2 |    23  Lily
        [3 rows x 2 columns]

    If the tuples are in fact `namedtuple`s, then the field names
    will be used for the column names in the resulting Frame. No
    check is made whether the named tuples in fact belong to the
    same class.

`List[Any]`
    If the list's first element does not match any of the cases
    above, then it is considered a "list of primitives". Such list
    will be parsed as a single column.

    The entries are typically `bool`s, `int`s, `float`s, `str`s,
    or `None`s; numpy scalars are also allowed. If the list has
    elements of heterogeneous types, then we will attempt to
    convert them to the smallest common stype.

    If the list contains only boolean values (or `None`s), then it
    will create a column of type `bool8`.

    If the list contains only integers (or `None`s), then the
    resulting column will be `int8` if all integers are 0 or 1; or
    `int32` if all entries are less than :math:`2^{31}` in magnitude;
    otherwise `int64` if all entries are less than :math:`2^{63}`
    in magnitude; or otherwise `float64`.

    If the list contains floats, then the resulting column will have
    stype `float64`. Both `None` and `math.nan` can be used to input
    NA values.

    Finally, if the list contains strings then the column produced
    will have stype `str32` if the total size of the character is
    less than 2Gb, or `str64` otherwise.

`typed_list`
    A typed list can be created by taking a regular list and
    dividing it by an stype. It behaves similarly to a simple
    list of primitives, except that it is parsed into the specific
    stype.

        >>> dt.Frame([1.5, 2.0, 3.87] / dt.float32).stype
        stype.float32

`Dict[str, Any]`
    The keys are column names, and values can be any objects from
    which a single-column frame can be constructed: list, range,
    np.array, single-column Frame, pandas series, etc.

    Constructing a frame from a dictionary `d` is exactly equivalent
    to calling `dt.Frame(list(d.values()), names=list(d.keys()))`.

`range`
    Same as if the range was expanded into a list of integers,
    except that the column created from a range is virtual and
    its creation time is nearly instant regardless of the range's
    length.

`Frame`
    If the argument is a :class:`Frame <datatable.Frame>`, then
    a shallow copy of that frame will be created, same as
    :meth:`.copy()`.

`str`
    If the source is a simple string, then the frame is created
    by :func:`fread <datatable.fread>`-ing this string.
    In particular, if the string contains the name of a file, the
    data will be loaded from that file; if it is a URL, the data
    will be downloaded and parsed from that URL. Lastly, the
    string may simply contain a table of data.

        >>> DT1 = dt.Frame("train.csv")
        >>> dt.Frame("""
        ...    Name    Age
        ...    Mary     39
        ...    Jasmine  17
        ...    Lily     23 """)
           | Name       Age
           | str32    int32
        -- + -------  -----
         0 | Mary        39
         1 | Jasmine     17
         2 | Lily        NA
        [3 rows x 2 columns]

`pd.DataFrame | pd.Series`
    A pandas DataFrame (Series) will be converted into a datatable
    Frame. Column names will be preserved.

    Column types will generally be the same, assuming they have a
    corresponding stype in datatable. If not, the column will be
    converted. For example, pandas date/time column will get converted
    into string, while `float16` will be converted into `float32`.

    If a pandas frame has an object column, we will attempt to refine
    it into a more specific stype. In particular, we can detect a
    string or boolean column stored as object in pandas.

`np.array`
    A numpy array will get converted into a Frame of the same shape
    (provided that it is 2- or less- dimensional) and the same type.

    If possible, we will create a Frame without copying the data
    (however, this is subject to numpy's approval). The resulting
    frame will have a copy-on-write semantics.

`pyarrow.Table`
    An arrow table will be converted into a datatable Frame, preserving
    column names and types.

    If the arrow table has columns of types not supported by datatable
    (for example lists or structs), an exception will be raised.

`None`
    When the source is not given at all, then a 0x0 frame will be
    created; unless a `names` parameter is provided, in which
    case the resulting frame will have 0 rows but as many columns
    as given in the `names` list.
)";

static PKArgs args___init__(1, 0, 3, false, true,
                            {"_data", "names", "stypes", "stype"},
                            "__init__", doc___init__);


static const char* doc_Frame =
R"(
Two-dimensional column-oriented container of data. This the primary
data structure in the :mod:`datatable` module.

A Frame is *two-dimensional* in the sense that it is comprised of
rows and columns of data. Each data cell can be located via a pair
of its coordinates: ``(irow, icol)``. We do not support frames with
more or less than two dimensions.

A Frame is *column-oriented* in the sense that internally the data is
stored separately for each column. Each column has its own name and
type. Types may be different for different columns but cannot vary
within each column.

Thus, the dimensions of a Frame are not symmetrical: a Frame is not
a matrix. Internally the class is optimized for the use case when the
number of rows significantly exceeds the number of columns.

A Frame can be viewed as a ``list`` of columns: standard Python
function ``len()`` will return the number of columns in the Frame,
and ``frame[j]`` will return the column at index ``j`` (each "column"
will be a Frame with ``ncols == 1``). Similarly, you can iterate over
the columns of a Frame in a loop, or use it in a ``*``-expansion::

    >>> for column in frame:
    ...    # do something
    ...
    >>> list_of_columns = [*frame]

A Frame can also be viewed as a ``dict`` of columns, where the key
associated with each column is its name. Thus, ``frame[name]`` will
return the column with the requested name. A Frame can also work with
standard python ``**``-expansion::

    >>> dict_of_columns = {**frame}
)";


void Frame::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.Frame");
  xt.set_class_doc(doc_Frame);
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
  xt.add(GETSET(&Frame::get_meta, &Frame::set_meta, args_meta));
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
