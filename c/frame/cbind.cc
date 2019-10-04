//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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

static PKArgs args_cbind(
  0, 0, 1, true, false,
  {"force"}, "cbind",

R"(cbind(self, *frames, force=False)
--

Append columns of Frames `frames` to the current Frame.

This is equivalent to `pandas.concat(axis=1)`: the Frames are combined
by columns, i.e. cbinding a Frame of shape [n x m] to a Frame of
shape [n x k] produces a Frame of shape [n x (m + k)].

As a special case, if you cbind a single-row Frame, then that row will
be replicated as many times as there are rows in the current Frame. This
makes it easy to create constant columns, or to append reduction results
(such as min/max/mean/etc) to the current Frame.

If Frame(s) being appended have different number of rows (with the
exception of Frames having 1 row), then the operation will fail by
default. You can force cbinding these Frames anyways by providing option
`force=True`: this will fill all 'short' Frames with NAs. Thus there is
a difference in how Frames with 1 row are treated compared to Frames
with any other number of rows.

Parameters
----------
frames: sequence or list of Frames
    One or more Frame to append. They should have the same number of
    rows (unless option `force` is also used).

force: boolean
    If True, allows Frames to be appended even if they have unequal
    number of rows. The resulting Frame will have number of rows equal
    to the largest among all Frames. Those Frames which have less
    than the largest number of rows, will be padded with NAs (with the
    exception of Frames having just 1 row, which will be replicated
    instead of filling with NAs).
)");



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
      throw ValueError() << "Cannot cbind frame with " << inrows << " rows to "
          "a frame with " << nrows << " rows. Use `force=True` to disregard "
          "this check and merge the frames anyways.";
    }
  }

  dt->cbind(datatables);
  _clear_types();
}




//------------------------------------------------------------------------------
// dt.cbind
//------------------------------------------------------------------------------

static PKArgs args_dt_cbind(
  0, 0, 1, true, false, {"force"}, "cbind", nullptr);

static oobj dt_cbind(const PKArgs& args) {
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
  ADD_FN(&dt_cbind, args_dt_cbind);
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

