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
#include "datatable.h"
#include "frame/py_frame.h"
#include "python/_all.h"
#include "utils/assert.h"

namespace py {


//------------------------------------------------------------------------------
// Frame::cbind
//------------------------------------------------------------------------------


// Forward-declare
static void check_nrows(DataTable* dt, size_t* nrows);
static Error item_error(const py::_obj&);



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



void Frame::cbind(const PKArgs& args) {
  bool force = args[0]? args[0].to_bool_strict() : false;

  size_t nrows = dt->nrows == 0 && dt->ncols == 0? size_t(-1) : dt->nrows;
  std::vector<DataTable*> dts;
  for (auto va : args.varargs()) {
    if (va.is_frame()) {
      DataTable* idt = va.to_frame();
      if (idt->ncols == 0) continue;
      if (!force) check_nrows(idt, &nrows);
      dts.push_back(idt);
    }
    else if (va.is_iterable()) {
      for (auto item : va.to_oiter()) {
        if (item.is_frame()) {
          DataTable* idt = item.to_frame();
          if (idt->ncols == 0) continue;
          if (!force) check_nrows(idt, &nrows);
          dts.push_back(idt);
        } else {
          throw item_error(item);
        }
      }
    } else {
      throw item_error(va);
    }
  }

  _clear_types();
  dt->cbind(dts);
}


static void check_nrows(DataTable* dt, size_t* nrows) {
  size_t inrows = dt->nrows;
  if (*nrows == 1 || *nrows == size_t(-1)) *nrows = inrows;
  if (*nrows == inrows || inrows == 1) return;
  throw ValueError() << "Cannot cbind frame with " << inrows << " rows to "
      "a frame with " << *nrows << " rows. Use `force=True` to disregard "
      "this check and merge the frames anyways.";
}

static Error item_error(const py::_obj& item) {
  return TypeError() << "Frame.cbind() expects a list or sequence of Frames, "
      "but got an argument of type " << item.typeobj();
}


void Frame::Type::_init_cbind(Methods& mm) {
  ADD_METHOD(mm, &Frame::cbind, args_cbind);
}



}  // namespace py


//------------------------------------------------------------------------------
// DataTable::cbind
//------------------------------------------------------------------------------

/**
 * Merge datatables `dts` into the datatable `dt`, by columns. Datatable `dt`
 * will be modified in-place. If any datatable has less rows than the others,
 * it will be filled with NAs; with the exception of 1-row datatables which will
 * be expanded to the desired height by duplicating that row.
 */
void DataTable::cbind(const std::vector<DataTable*>& dts)
{
  size_t t_ncols = ncols;
  size_t t_nrows = nrows;
  for (auto dt : dts) {
    t_ncols += dt->ncols;
    if (t_nrows < dt->nrows) t_nrows = dt->nrows;
  }

  // Fix up the main datatable if it has too few rows
  if (nrows < t_nrows) {
    for (auto col : columns) {
      col->resize_and_fill(t_nrows);
    }
    nrows = t_nrows;
  }

  // Append columns from `dts` into the "main" datatable
  //
  // NOTE: when appending a DataTable to itself, the following happens:
  // we start changing `this->columns` vector, which thus becomes
  // temporarily out-of-sync with `this->ncols` field (which reflects the
  // original number of columns). Thus, when iterating over columns of
  // `dt`, it is important NOT to use `for (col : dt->columns)` iterator.
  //
  std::vector<std::string> newnames = names;
  columns.reserve(t_ncols);
  for (auto dt : dts) {
    size_t ncolsi = dt->ncols;
    bool fix_columns = (dt->nrows < t_nrows);
    for (size_t ii = 0; ii < ncolsi; ++ii) {
      Column* c = dt->columns[ii]->shallowcopy();
      if (fix_columns) c->resize_and_fill(t_nrows);
      columns.push_back(c);
    }
    const auto& namesi = dt->names;
    xassert(namesi.size() == ncolsi);
    newnames.insert(newnames.end(), namesi.begin(), namesi.end());
  }
  xassert(columns.size() == t_ncols);
  xassert(newnames.size() == t_ncols);

  // Done.
  ncols = t_ncols;
  set_names(newnames);
}
