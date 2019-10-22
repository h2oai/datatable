//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#include <algorithm>
#include <functional>   // std::function
#include <numeric>
#include <unordered_map>
#include "column/sentinel_fw.h"
#include "column/sentinel_str.h"
#include "frame/py_frame.h"
#include "python/_all.h"
#include "utils/assert.h"
#include "utils/misc.h"
#include "datatable.h"
#include "datatablemodule.h"


static void _check_ncols(size_t n0, size_t n1) {
  if (n0 == n1) return;
  throw ValueError() << "Cannot rbind frame with "
      << n1 << " column" << (n1==1? "" : "s") << " to a frame with "
      << n0 << " column" << (n0==1? "" : "s") << " without parameter "
      "`force=True`";
}


static constexpr size_t INVALID_INDEX = size_t(-1);



//------------------------------------------------------------------------------
// Frame::rbind
//------------------------------------------------------------------------------
namespace py {

static PKArgs args_rbind(
  0, 0, 2, true, false,
  {"force", "bynames"}, "rbind",

R"(rbind(self, *frames, force=False, bynames=True)
--

Append rows of `frames` to the current frame.

This is equivalent to `list.extend()` in Python: the frames are
combined by rows, i.e. rbinding a frame of shape [n x k] to a Frame
of shape [m x k] produces a frame of shape [(m + n) x k].

This method modifies the current frame in-place. If you do not want
the current frame modified, then use `dt.rbind()` function.

If frame(s) being appended have columns of types different from the
current frame, then these columns will be promoted to the largest of
their types: bool -> int -> float -> string.

If you need to append multiple frames, then it is more efficient to
collect them into an array first and then do a single `rbind()`, than
it is to append them one-by-one.

Appending data to a frame opened from disk will force loading the
current frame into memory, which may fail with an OutOfMemory
exception if the frame is sufficiently big.

Parameters
----------
frames: sequence or list of Frames
    One or more frame to append. These frames should have the same
    columnar structure as the current frame (unless option `force` is
    used).

force: bool
    If True, then the frames are allowed to have mismatching set of
    columns. Any gaps in the data will be filled with NAs.

bynames: bool
    If True (default), the columns in frames are matched by their
    names. For example, if one frame has columns ["colA", "colB",
    "colC"] and the other ["colB", "colA", "colC"] then we will swap
    the order of the first two columns of the appended frame before
    performing the append. However if `bynames` is False, then the
    column names will be ignored, and the columns will be matched
    according to their order, i.e. i-th column in the current frame
    to the i-th column in each appended frame.
)");


void Frame::rbind(const PKArgs& args) {
  bool force = args[0].to<bool>(false);
  bool bynames = args[1].to<bool>(true);

  std::vector<DataTable*> dts;
  std::vector<intvec> cols;

  // First, find all frames that will be rbound. We will process both the
  // vararg sequence, and the case when a list (or tuple) passed. In fact,
  // we even allow a sequence of lists, because why not.
  // Any frames with 0 rows will be disregarded.
  {
    size_t j = 0;
    std::function<void(py::robj)> process_arg = [&](py::robj arg) {
      if (arg.is_frame()) {
        DataTable* df = arg.to_datatable();
        if (df->nrows()) dts.push_back(df);
        ++j;
      }
      else if (arg.is_iterable()) {
        for (auto item : arg.to_oiter()) {
          process_arg(item);
        }
      }
      else {
        throw TypeError() << "`Frame.rbind()` expects a list or sequence of "
            "Frames as an argument; instead item " << j
            << " was a " << arg.typeobj();
      }
    };

    for (auto arg : args.varargs()) {
      process_arg(arg);
    }
  }

  // Ignore trivial case
  if (dts.empty()) return;
  if (dt->nkeys() > 0) {
    throw ValueError() << "Cannot rbind to a keyed frame";
  }

  strvec final_names = dt->get_names();  // copy the names
  size_t n = dt->ncols();
  if (n == 0) {
    final_names = dts[0]->get_names();
    n = final_names.size();
  }

  // Fix up the shape of `cols`: it has to be a rectangular matrix with the
  // number of rows = `dts.size()`, and the number of columns = `n`. Note
  // that so far `n` is just the numer of columns in the current frame. The
  // output may end up containing more columns than this.
  cols.resize(n);
  for (auto& cc : cols) {
    cc.resize(dts.size(), INVALID_INDEX);
  }

  // Process the case when the frames are matched by column names
  if (bynames) {
    std::unordered_map<std::string, size_t> inames;
    size_t i = 0;
    for (const auto& name : final_names) {
      inames[name] = i++;
    }
    i = 0;
    for (DataTable* df : dts) {
      if (!force) _check_ncols(n, df->ncols());
      const strvec& dfnames = df->get_names();
      size_t j = 0;
      for (const auto& name : dfnames) {
        if (j < n && name == final_names[j]) {
          cols[j][i] = j;
        }
        else if (inames.count(name)) {
          cols[inames[name]][i] = j;
        }
        else if (force) {
          final_names.push_back(name);
          cols.push_back(intvec(dts.size(), INVALID_INDEX));
          inames[name] = n;
          cols[n][i] = j;
          ++n;
          xassert(final_names.size() == n);
        }
        else {
          throw ValueError() << "Column `" << name << "` is not found "
              "in the original frame; if you want to rbind the frames anyways "
              "filling missing values with NAs, then use `force=True`";
        }
        ++j;
      }
      ++i;
    }
  }

  // Otherwise the columns are matched simply by their order
  else {
    size_t i = 0;
    for (DataTable* df : dts) {
      if (n != df->ncols()) {
        if (!force) _check_ncols(n, df->ncols());
        if (n < df->ncols()) {
          const strvec& dfnames = df->get_names();
          for (size_t j = n; j < df->ncols(); ++j) {
            final_names.push_back(dfnames[j]);
            cols.push_back(intvec(dts.size(), INVALID_INDEX));
          }
          n = df->ncols();
        }
      }
      for (size_t j = 0; j < df->ncols(); ++j) {
        cols[j][i] = j;
      }
      ++i;
    }
  }

  _clear_types();
  dt->rbind(dts, cols);
  dt->set_names(final_names);
}




//------------------------------------------------------------------------------
// dt.rbind
//------------------------------------------------------------------------------

static PKArgs args_dt_rbind(
  0, 0, 2, true, false, {"force", "bynames"}, "rbind", nullptr);

static oobj dt_rbind(const PKArgs& args) {
  oobj r = oobj::import("datatable", "Frame").call();
  PyObject* rv = r.to_borrowed_ref();
  reinterpret_cast<Frame*>(rv)->rbind(args);
  return r;
}



void Frame::_init_rbind(XTypeMaker& xt) {
  xt.add(METHOD(&Frame::rbind, args_rbind));
}


void DatatableModule::init_methods_rbind() {
  ADD_FN(&dt_rbind, args_dt_rbind);
}

}  // namespace py




//------------------------------------------------------------------------------
// DataTable::rbind
//------------------------------------------------------------------------------

/**
 * Append to this Frame a list of other Frames `dts`. The `col_indices` array
 * specifies how the columns should be matched.
 *
 * In particular, the Frame `dt` will be expanded to have `col_indices.size()`
 * columns, and `dt->nrows + sum(dti->nrows for dti in dts)` rows. The `i`th
 * column in the expanded Frame will have the following structure: first comes
 * the data from `i`th column of `dt` (if `i < dt->ncols`, otherwise NAs); after
 * that come `ndts` blocks of rows, each `j`th block having data from column
 * number `col_indices[i][j]` in Frame `dts[j]` (if `col_indices[i][j] >= 0`,
 * otherwise NAs).
 */
void DataTable::rbind(
    const std::vector<DataTable*>& dts,
    const std::vector<intvec>& col_indices)
{
  size_t new_ncols = col_indices.size();
  xassert(new_ncols >= ncols_);
  xassert(nkeys_ == 0);

  columns_.reserve(new_ncols);
  for (size_t i = ncols_; i < new_ncols; ++i) {
    columns_.push_back(Column::new_na_column(nrows_));
  }

  size_t new_nrows = nrows_;
  for (auto dt : dts) {
    new_nrows += dt->nrows();
  }

  colvec cols_to_append(dts.size());
  for (size_t i = 0; i < new_ncols; ++i) {
    for (size_t j = 0; j < dts.size(); ++j) {
      size_t k = col_indices[i][j];
      Column col = (k == INVALID_INDEX)
                      ? Column::new_na_column(dts[j]->nrows())
                      : dts[j]->get_column(k);
      cols_to_append[j] = std::move(col);
    }
    columns_[i].rbind(cols_to_append);
  }
  ncols_ = new_ncols;
  nrows_ = new_nrows;
}




//------------------------------------------------------------------------------
//  Column::rbind()
//------------------------------------------------------------------------------

void Column::rbind(colvec& columns) {
  _get_mutable_impl();
  // Is the current column "empty" ?
  bool col_empty = (stype() == SType::VOID);
  if (!col_empty) this->materialize();

  // Compute the final number of rows and stype
  size_t new_nrows = nrows();
  SType new_stype = col_empty? SType::BOOL : stype();
  for (auto& col : columns) {
    col.materialize();
    new_nrows += col.nrows();
    // TODO: need better stype promotion mechanism than mere max()
    new_stype = std::max(new_stype, col.stype());
  }

  // Create the resulting Column object. It can be either: an empty column
  // filled with NAs; the current column; or a type-cast of the current column.
  Column newcol;
  if (col_empty) {
    newcol = Column::new_na_column(nrows(), new_stype);
  } else if (stype() == new_stype) {
    newcol = std::move(*this);
  } else {
    newcol = cast(new_stype);
  }
  xassert(newcol.stype() == new_stype);

  // TODO: Temporary Fix. To be resolved in #301
  newcol.reset_stats();

  // Use the appropriate strategy to continue appending the columns.
  newcol.materialize();
  newcol._get_mutable_impl()->rbind_impl(columns, new_nrows, col_empty);

  // Replace current column's impl with the newcol's
  std::swap(impl_, newcol.impl_);
}



//------------------------------------------------------------------------------
// rbind string columns
//------------------------------------------------------------------------------

template <typename T>
void dt::SentinelStr_ColumnImpl<T>::rbind_impl(colvec& columns, size_t new_nrows,
                                 bool col_empty)
{
  // Determine the size of the memory to allocate
  size_t old_nrows = nrows_;
  size_t new_strbuf_size = 0;     // size of the string data region
  if (!col_empty) {
    new_strbuf_size += strbuf_.size();
  }
  for (size_t i = 0; i < columns.size(); ++i) {
    Column& col = columns[i];
    if (col.stype() == SType::VOID) continue;
    if (col.ltype() != LType::STRING) {
      col = col.cast(stype_);
    }
    new_strbuf_size += col.get_data_size(1);
  }
  size_t new_mbuf_size = sizeof(T) * (new_nrows + 1);

  // Reallocate the column
  offbuf_.resize(new_mbuf_size);
  strbuf_.resize(new_strbuf_size);
  nrows_ = new_nrows;
  T* offs = static_cast<T*>(offbuf_.wptr()) + 1;

  // Move the original offsets
  size_t rows_to_fill = 0;  // how many rows need to be filled with NAs
  T curr_offset = 0;        // Current offset within string data section
  offs[-1] = 0;
  if (col_empty) {
    rows_to_fill += old_nrows;
  } else {
    curr_offset = offs[old_nrows - 1] & ~GETNA<T>();
    offs += old_nrows;
  }
  for (Column& col : columns) {
    if (col.stype() == SType::VOID) {
      rows_to_fill += col.nrows();
    } else {
      if (rows_to_fill) {
        const T na = curr_offset ^ GETNA<T>();
        std::fill(offs, offs + rows_to_fill, na);
        offs += rows_to_fill;
        rows_to_fill = 0;
      }
      const void* col_maindata = col.get_data_readonly(0);
      const T delta_na = static_cast<T>(GETNA<uint64_t>() -
                                            GETNA<uint32_t>());
      if (col.stype() == SType::STR32) {
        auto col_offsets = reinterpret_cast<const uint32_t*>(col_maindata) + 1;
        for (size_t j = 0; j < col.nrows(); ++j) {
          uint32_t offset = col_offsets[j];
          *offs++ = static_cast<T>(offset) + curr_offset +
                    (sizeof(T)==8 && ISNA<uint32_t>(offset)? delta_na : 0);
        }
      } else {
        xassert(col.stype() == SType::STR64);
        auto col_offsets = reinterpret_cast<const uint64_t*>(col_maindata) + 1;
        for (size_t j = 0; j < col.nrows(); ++j) {
          uint64_t offset = col_offsets[j];
          *offs++ = static_cast<T>(offset) + curr_offset -
                    (sizeof(T)==4 && ISNA<uint64_t>(offset)? delta_na : 0);
        }
      }
      const void* col_strdata = col.get_data_readonly(1);
      const size_t col_strsize = col.get_data_size(1);
      if (col_strsize) {
        void* target = strbuf_.wptr(static_cast<size_t>(curr_offset));
        std::memcpy(target, col_strdata, col_strsize);
        curr_offset += col_strsize;
      }
    }
  }
  if (rows_to_fill) {
    const T na = curr_offset ^ GETNA<T>();
    set_value(offs, &na, sizeof(T), rows_to_fill);
  }
}



//------------------------------------------------------------------------------
// rbind fixed-width columns
//------------------------------------------------------------------------------
template<> inline py::robj GETNA() { return py::rnone(); }

template <typename T>
void dt::SentinelFw_ColumnImpl<T>::rbind_impl(colvec& columns, size_t new_nrows, bool col_empty)
{
  const T na = GETNA<T>();
  const void* naptr = static_cast<const void*>(&na);

  // Reallocate the column's data buffer
  size_t old_nrows = nrows_;
  size_t old_alloc_size = sizeof(T) * old_nrows;
  size_t new_alloc_size = sizeof(T) * new_nrows;
  mbuf_.resize(new_alloc_size);
  nrows_ = new_nrows;

  // Copy the data
  char* resptr = static_cast<char*>(mbuf_.wptr());
  char* resptr0 = resptr;
  size_t rows_to_fill = 0;
  if (col_empty) {
    rows_to_fill = old_nrows;
  } else {
    resptr += old_alloc_size;
  }
  for (auto& col : columns) {
    if (col.stype() == SType::VOID) {
      rows_to_fill += col.nrows();
    } else {
      if (rows_to_fill) {
        set_value(resptr, naptr, sizeof(T), rows_to_fill);
        resptr += rows_to_fill * sizeof(T);
        rows_to_fill = 0;
      }
      if (col.stype() != stype_) {
        col.cast_inplace(stype_);
      }
      size_t col_data_size = sizeof(T) * col.nrows();
      std::memcpy(resptr, col.get_data_readonly(), col_data_size);
      resptr += col_data_size;
    }
  }
  if (rows_to_fill) {
    set_value(resptr, naptr, sizeof(T), rows_to_fill);
    resptr += rows_to_fill * sizeof(T);
  }
  xassert(resptr == resptr0 + new_alloc_size);
  (void)resptr0;
}



//------------------------------------------------------------------------------
// rbind object columns
//------------------------------------------------------------------------------

void dt::SentinelObj_ColumnImpl::rbind_impl(
  colvec& columns, size_t nnrows, bool col_empty)
{
  size_t old_nrows = nrows_;
  size_t new_nrows = nnrows;

  // Reallocate the column's data buffer
  // `resize` fills all new elements with Py_None
  mbuf_.resize(sizeof(PyObject*) * new_nrows);
  nrows_ = nnrows;

  // Copy the data
  PyObject** dest_data = static_cast<PyObject**>(mbuf_.wptr());
  PyObject** dest_data0 = dest_data;
  if (!col_empty) {
    dest_data += old_nrows;
  }
  for (auto& col : columns) {
    if (col.stype() == SType::VOID) {
      dest_data += col.nrows();
    } else {
      if (col.stype() != SType::OBJ) {
        col = col.cast(stype_);
      }
      auto src_data = static_cast<PyObject* const*>(
                        col.get_data_readonly());
      for (size_t i = 0; i < col.nrows(); ++i) {
        Py_INCREF(src_data[i]);
        Py_DECREF(*dest_data);
        *dest_data = src_data[i];
        dest_data++;
      }
    }
  }
  xassert(dest_data == dest_data0 + new_nrows);
  (void)dest_data0;
}


// Explicitly instantiate these templates:
template class dt::SentinelFw_ColumnImpl<int8_t>;
template class dt::SentinelFw_ColumnImpl<int16_t>;
template class dt::SentinelFw_ColumnImpl<int32_t>;
template class dt::SentinelFw_ColumnImpl<int64_t>;
template class dt::SentinelFw_ColumnImpl<float>;
template class dt::SentinelFw_ColumnImpl<double>;
template class dt::SentinelFw_ColumnImpl<py::robj>;
template class dt::SentinelStr_ColumnImpl<uint32_t>;
template class dt::SentinelStr_ColumnImpl<uint64_t>;
