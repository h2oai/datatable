//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include "datatable.h"
#include "datatablemodule.h"
#include "documentation.h"
#include "frame/py_frame.h"
#include "python/_all.h"
#include "python/xargs.h"
#include "utils/assert.h"

namespace py {


//------------------------------------------------------------------------------
// Frame::cbind
//------------------------------------------------------------------------------

void Frame::cbind(const XArgs& args) {
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

DECLARE_METHODv(&Frame::cbind)
    ->name("cbind")
    ->docs(dt::doc_Frame_cbind)
    ->n_keyword_args(1)
    ->allow_varargs()
    ->arg_names({"force"});



//------------------------------------------------------------------------------
// dt.cbind
//------------------------------------------------------------------------------

static oobj py_cbind(const XArgs& args) {
  oobj r = oobj::import("datatable", "Frame").call();
  xassert(r.is_frame());
  PyObject* rv = r.to_borrowed_ref();
  reinterpret_cast<Frame*>(rv)->cbind(args);
  return r;
}

DECLARE_PYFN(&py_cbind)
    ->name("cbind")
    ->n_keyword_args(1)
    ->allow_varargs()
    ->arg_names({"force"})
    ->docs(dt::doc_dt_cbind);




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

