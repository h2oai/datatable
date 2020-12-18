//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//------------------------------------------------------------------------------
#include <functional>
#include "frame/py_frame.h"
#include "python/_all.h"
#include "utils/assert.h"
#include "datatable.h"
#include "datatablemodule.h"

namespace py {


//------------------------------------------------------------------------------
// Frame::cbind
//------------------------------------------------------------------------------

static const char* doc_cbind =
R"(cbind(self, *frames, force=False)
--

Append columns of one or more `frames` to the current Frame.

For example, if the current frame has `n` columns, and you are
appending another frame with `k` columns, then after this method
succeeds, the current frame will have `n + k` columns. Thus, this
method is roughly equivalent to `pandas.concat(axis=1)`.

The frames being cbound must all either have the same number of rows,
or some of them may have only a single row. Such single-row frames
will be automatically expanded, replicating the value as needed.
This makes it easy to create constant columns or to append reduction
results (such as min/max/mean/etc) to the current Frame.

If some of the `frames` have an incompatible number of rows, then the
operation will fail with an :exc:`dt.exceptions.InvalidOperationError`.
However, if you set the flag `force` to True, then the error will no
longer be raised - instead all frames that are shorter than the others
will be padded with NAs.

If the frames being appended have the same column names as the current
frame, then those names will be :ref:`mangled <name-mangling>`
to ensure that the column names in the current frame remain unique.
A warning will also be issued in this case.


Parameters
----------
frames: Frame | List[Frame] | None
    The list/tuple/sequence/generator expression of Frames to append
    to the current frame. The list may also contain `None` values,
    which will be simply skipped.

force: bool
    If True, allows Frames to be appended even if they have unequal
    number of rows. The resulting Frame will have number of rows equal
    to the largest among all Frames. Those Frames which have less
    than the largest number of rows, will be padded with NAs (with the
    exception of Frames having just 1 row, which will be replicated
    instead of filling with NAs).

return: None
    This method alters the current frame in-place, and doesn't return
    anything.

except: InvalidOperationError
    If trying to cbind frames with the number of rows different from
    the current frame's, and the option `force` is not set.


Notes
-----

Cbinding frames is a very cheap operation: the columns are copied by
reference, which means the complexity of the operation depends only
on the number of columns, not on the number of rows. Still, if you
are planning to cbind a large number of frames, it will be beneficial
to collect them in a list first and then call a single `cbind()`
instead of cbinding them one-by-one.

It is possible to cbind frames using the standard `DT[i,j]` syntax::

    >>> df[:, update(**frame1, **frame2, ...)]

Or, if you need to append just a single column::

    >>> df["newcol"] = frame1


Examples
--------
>>> DT = dt.Frame(A=[1, 2, 3], B=[4, 7, 0])
>>> frame1 = dt.Frame(N=[-1, -2, -5])
>>> DT.cbind(frame1)
>>> DT
   |     A      B      N
   | int32  int32  int32
-- + -----  -----  -----
 0 |     1      4     -1
 1 |     2      7     -2
 2 |     3      0     -5
[3 rows x 3 columns]


See also
--------
- :func:`datatable.cbind` -- function for cbinding frames
  "out-of-place" instead of in-place;

- :meth:`.rbind()` -- method for row-binding frames.
)";

static PKArgs args_cbind(0, 0, 1, true, false, {"force"}, "cbind", doc_cbind);



void Frame::cbind(const PKArgs& args)
{
  std::vector<py::oobj> frame_objs;
  std::vector<DataTable*> datatables;

  std::function<void(robj)> collect_arg = [&](robj item) -> void {
    if (item.is_frame()) {
      DataTable* idt = item.to_datatable();
      if (idt->ncols() == 0) return;
      frame_objs.emplace_back(item);
      datatables.push_back(idt);
    }
    else if (item.is_list_or_tuple() || item.is_generator()) {
      for (auto subitem : item.to_oiter()) {
        collect_arg(subitem);
      }
    } else if (item.is_none()) {
      return;
    } else {
      throw TypeError() << "Frame.cbind() expects a list or sequence of "
          "Frames, but got an argument of type " << item.typeobj();
    }
  };

  for (auto va : args.varargs()) {
    collect_arg(va);
  }

  bool force = args[0]? args[0].to_bool_strict() : false;
  if (!force) {
    size_t nrows = (dt->nrows() == 0 && dt->ncols() == 0)? 1 : dt->nrows();
    for (DataTable* idt : datatables) {
      size_t inrows = idt->nrows();
      if (nrows == 1) nrows = inrows;
      if (nrows == inrows || inrows == 1) continue;
      throw InvalidOperationError() <<
          "Cannot cbind frame with " << inrows << " rows to a frame with " <<
          nrows << " rows";
    }
  }

  dt->cbind(datatables);
  _clear_types();
}




//------------------------------------------------------------------------------
// dt.cbind
//------------------------------------------------------------------------------

static const char* doc_py_cbind =
R"(cbind(*frames, force=False)
--

Create a new Frame by appending columns from several `frames`.

This function is exactly equivalent to::

    >>> dt.Frame().cbind(*frames, force=force)

Parameters
----------
frames: Frame | List[Frame] | None

force: bool

return: Frame


See also
--------
- :func:`rbind()` -- function for row-binding several frames.
- :meth:`dt.Frame.cbind()` -- Frame method for cbinding some frames to
  another.


Examples
--------
.. code-block:: python

    >>> from datatable import dt, f
    >>>
    >>> DT = dt.Frame(A=[1, 2, 3], B=[4, 7, 0])
    >>> DT
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     1      4
     1 |     2      7
     2 |     3      0
    [3 rows x 2 columns]

    >>> frame1 = dt.Frame(N=[-1, -2, -5])
    >>> frame1
       |     N
       | int32
    -- + -----
     0 |    -1
     1 |    -2
     2 |    -5
    [3 rows x 1 column]

    >>> dt.cbind([DT, frame1])
       |     A      B      N
       | int32  int32  int32
    -- + -----  -----  -----
     0 |     1      4     -1
     1 |     2      7     -2
     2 |     3      0     -5
    [3 rows x 3 columns]

If the number of rows are not equal, you can force the binding by setting
the `force` parameter to `True`::

    >>> frame2 = dt.Frame(N=[-1, -2, -5, -20])
    >>> frame2
       |     N
       | int32
    -- + -----
     0 |    -1
     1 |    -2
     2 |    -5
     3 |   -20
    [4 rows x 1 column]

    >>> dt.cbind([DT, frame2], force=True)
       |     A      B      N
       | int32  int32  int32
    -- + -----  -----  -----
     0 |     1      4     -1
     1 |     2      7     -2
     2 |     3      0     -5
     3 |    NA     NA    -20
    [4 rows x 3 columns]
)";

static PKArgs args_py_cbind(
  0, 0, 1, true, false, {"force"}, "cbind", doc_py_cbind);

static oobj py_cbind(const PKArgs& args) {
  oobj r = oobj::import("datatable", "Frame").call();
  xassert(r.is_frame());
  PyObject* rv = r.to_borrowed_ref();
  reinterpret_cast<Frame*>(rv)->cbind(args);
  return r;
}



void Frame::_init_cbind(XTypeMaker& xt) {
  xt.add(METHOD(&Frame::cbind, args_cbind));
}

void DatatableModule::init_methods_cbind() {
  ADD_FN(&py_cbind, args_py_cbind);
}




}  // namespace py
//------------------------------------------------------------------------------
// DataTable::cbind
//------------------------------------------------------------------------------

/**
 * Merge `datatables` into the current DataTable, modifying it in-place. If any
 * DataTable has less rows than the others, it will be filled with NAs; with
 * the exception of 1-row datatables which will be expanded to the desired
 * height by duplicating that row.
 */
void DataTable::cbind(const std::vector<DataTable*>& datatables)
{
  size_t final_ncols = ncols_;
  size_t final_nrows = nrows_;
  for (DataTable* dt : datatables) {
    final_ncols += dt->ncols();
    if (final_nrows < dt->nrows()) final_nrows = dt->nrows();
  }

  bool fix_columns = (nrows_ < final_nrows);
  strvec newnames = names_;
  columns_.reserve(final_ncols);

  // NOTE: when appending a DataTable to itself, the following happens:
  // we start changing `this->columns` vector, which thus becomes
  // temporarily out-of-sync with `this->ncols` field (which reflects the
  // original number of columns). Thus, when iterating over columns of
  // `dt`, it is important NOT to use `for (col : dt->columns)` iterator.
  //
  for (auto dt : datatables) {
    fix_columns |= (dt->nrows() < final_nrows);
    for (size_t ii = 0; ii < dt->ncols(); ++ii) {
      columns_.push_back(dt->columns_[ii]);
    }
    const strvec& namesi = dt->get_names();
    xassert(namesi.size() == dt->ncols());
    newnames.insert(newnames.end(), namesi.begin(), namesi.end());
  }
  xassert(columns_.size() == final_ncols);
  xassert(newnames.size() == final_ncols);

  // Fix up the DataTable's columns if they have different number of rows
  if (fix_columns) {
    for (Column& col : columns_) {
      if (col.nrows() == 1) {
        col.repeat(final_nrows);
      } else {
        col.resize(final_nrows);  // padding with NAs
      }
    }
  }

  // Done.
  nrows_ = final_nrows;
  ncols_ = final_ncols;
  set_names(newnames);
}

