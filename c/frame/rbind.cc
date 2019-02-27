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
#include <numeric>
#include <unordered_map>
#include "frame/py_frame.h"
#include "python/_all.h"
#include "utils/assert.h"
#include "datatable.h"
#include "datatablemodule.h"

static void _check_ncols(size_t n0, size_t n1) {
  if (n0 == n1) return;
  throw ValueError() << "Cannot rbind frame with "
      << n1 << " column" << (n1==1? "" : "s") << " to a frame with "
      << n0 << " column" << (n0==1? "" : "s") << " without parameter "
      "`force=True`";
}

[[noreturn]] static void _notframe_error(size_t i, py::robj obj) {
  throw TypeError() << "`Frame.rbind()` expects a list or sequence of Frames "
      "as an argument; instead item " << i << " was a " << obj.typeobj();
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
    for (auto arg : args.varargs()) {
      if (arg.is_frame()) {
        DataTable* df = arg.to_frame();
        if (df->nrows) dts.push_back(df);
        ++j;
      }
      else if (arg.is_list_or_tuple()) {
        olist list = arg.to_pylist();
        for (size_t i = 0; i < list.size(); ++i) {
          auto item = list[i];
          if (!item.is_frame()) {
            _notframe_error(j, item);
          }
          DataTable* df = item.to_frame();
          if (df->nrows) dts.push_back(df);
          ++j;
        }
      }
      else {
        _notframe_error(j, arg);
      }
    }
  }

  // Ignore trivial case
  if (dts.empty()) return;

  strvec final_names = dt->get_names();  // copy the names
  size_t n = dt->ncols;
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
      if (!force) _check_ncols(n, df->ncols);
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
      if (n != df->ncols) {
        if (!force) _check_ncols(n, df->ncols);
        if (n < df->ncols) {
          const strvec& dfnames = df->get_names();
          for (size_t j = n; j < df->ncols; ++j) {
            final_names.push_back(dfnames[j]);
            cols.push_back(intvec(dts.size(), INVALID_INDEX));
          }
          n = df->ncols;
        }
      }
      for (size_t j = 0; j < df->ncols; ++j) {
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



void Frame::Type::_init_rbind(Methods& mm) {
  ADD_METHOD(mm, &Frame::rbind, args_rbind);
}


void DatatableModule::init_methods_rbind() {
  ADD_FN(&dt_rbind, args_dt_rbind);
}

}  // namespace py




//------------------------------------------------------------------------------
// DataTable::rbind
//------------------------------------------------------------------------------

/**
 * Append to this Frame a list of other Frames `dts`. The `cols` array of
 * specifies how the columns should be matched.
 *
 * In particular, the Frame `dt` will be expanded to have `cols.size()` columns,
 * and `dt->nrows + sum(dti->nrows for dti in dts)` rows. The `i`th column
 * in the expanded Frame will have the following structure: first comes the
 * data from `i`th column of `dt` (if `i < dt->ncols`, otherwise NAs); after
 * that come `ndts` blocks of rows, each `j`th block having data from column
 * number `cols[i][j]` in Frame `dts[j]` (if `cols[i][j] >= 0`, otherwise
 * NAs).
 */
void DataTable::rbind(
    const std::vector<DataTable*>& dts, const std::vector<intvec>& cols)
{
  size_t new_ncols = cols.size();
  xassert(new_ncols >= ncols);

  // If this is a view Frame, then it must be materialized.
  this->reify();

  columns.resize(new_ncols, nullptr);
  for (size_t i = ncols; i < new_ncols; ++i) {
    columns[i] = new VoidColumn(nrows);
  }

  size_t new_nrows = nrows;
  for (auto dt : dts) {
    new_nrows += dt->nrows;
  }

  std::vector<const Column*> cols_to_append(dts.size(), nullptr);
  for (size_t i = 0; i < new_ncols; ++i) {
    for (size_t j = 0; j < dts.size(); ++j) {
      size_t k = cols[i][j];
      Column* col = k == INVALID_INDEX
                      ? new VoidColumn(dts[j]->nrows)
                      : dts[j]->columns[k]->shallowcopy();
      col->reify();
      cols_to_append[j] = col;
    }
    columns[i] = columns[i]->rbind(cols_to_append);
  }
  ncols = new_ncols;
  nrows = new_nrows;
}




//------------------------------------------------------------------------------
//  Column::rbind()
//------------------------------------------------------------------------------

Column* Column::rbind(std::vector<const Column*>& columns)
{
  // Is the current column "empty" ?
  bool col_empty = (stype() == SType::VOID);
  // Compute the final number of rows and stype
  size_t new_nrows = this->nrows;
  SType new_stype = col_empty? SType::BOOL : stype();
  for (const Column* col : columns) {
    new_nrows += col->nrows;
    new_stype = std::max(new_stype, col->stype());
  }

  // Create the resulting Column object. It can be either: an empty column
  // filled with NAs; the current column (`this`); a clone of the current
  // column (if it has refcount > 1); or a type-cast of the current column.
  Column* res = nullptr;
  if (col_empty) {
    res = Column::new_na_column(new_stype, this->nrows);
  } else if (stype() == new_stype) {
    res = this;
  } else {
    res = this->cast(new_stype);
  }
  xassert(res->stype() == new_stype);

  // TODO: Temporary Fix. To be resolved in #301
  if (res->stats != nullptr) res->stats->reset();

  // Use the appropriate strategy to continue appending the columns.
  res->rbind_impl(columns, new_nrows, col_empty);

  // If everything is fine, then the current column can be safely discarded
  // -- the upstream caller will replace this column with the `res`.
  if (res != this) delete this;
  return res;
}




//------------------------------------------------------------------------------
// rbind string columns
//------------------------------------------------------------------------------

template <typename T>
void StringColumn<T>::rbind_impl(std::vector<const Column*>& columns,
                                 size_t new_nrows, bool col_empty)
{
  // Determine the size of the memory to allocate
  size_t old_nrows = nrows;
  size_t new_strbuf_size = 0;     // size of the string data region
  if (!col_empty) {
    new_strbuf_size += strbuf.size();
  }
  for (size_t i = 0; i < columns.size(); ++i) {
    const Column* col = columns[i];
    if (col->stype() == SType::VOID) continue;
    if (col->stype() != stype()) {
      columns[i] = col->cast(stype());
      delete col;
      col = columns[i];
    }
    // TODO: replace with datasize(). But: what if col is not a string?
    new_strbuf_size += static_cast<const StringColumn<T>*>(col)->strbuf.size();
  }
  size_t new_mbuf_size = sizeof(T) * (new_nrows + 1);

  // Reallocate the column
  mbuf.resize(new_mbuf_size);
  strbuf.resize(new_strbuf_size);
  nrows = new_nrows;
  T* offs = offsets_w();

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
  for (const Column* col : columns) {
    if (col->stype() == SType::VOID) {
      rows_to_fill += col->nrows;
    } else {
      if (rows_to_fill) {
        const T na = curr_offset | GETNA<T>();
        set_value(offs, &na, sizeof(T), rows_to_fill);
        offs += rows_to_fill;
        rows_to_fill = 0;
      }
      const T* col_offsets = static_cast<const StringColumn<T>*>(col)->offsets();
      size_t col_nrows = col->nrows;
      for (size_t j = 0; j < col_nrows; ++j) {
        T off = col_offsets[j];
        *offs++ = off + curr_offset;
      }
      auto strcol = static_cast<const StringColumn<T>*>(col);
      size_t sz = strcol->strbuf.size();
      if (sz) {
        void* target = strbuf.wptr(static_cast<size_t>(curr_offset));
        std::memcpy(target, strcol->strbuf.rptr(), sz);
        curr_offset += sz;
      }
    }
    delete col;
  }
  if (rows_to_fill) {
    const T na = curr_offset | GETNA<T>();
    set_value(offs, &na, sizeof(T), rows_to_fill);
  }
}



//------------------------------------------------------------------------------
// rbind fixed-width columns
//------------------------------------------------------------------------------

template <typename T>
void FwColumn<T>::rbind_impl(std::vector<const Column*>& columns,
                             size_t new_nrows, bool col_empty)
{
  const T na = na_elem;
  const void* naptr = static_cast<const void*>(&na);

  // Reallocate the column's data buffer
  size_t old_nrows = nrows;
  size_t old_alloc_size = alloc_size();
  size_t new_alloc_size = sizeof(T) * new_nrows;
  mbuf.resize(new_alloc_size);
  nrows = new_nrows;

  // Copy the data
  char* resptr = static_cast<char*>(mbuf.wptr());
  char* resptr0 = resptr;
  size_t rows_to_fill = 0;
  if (col_empty) {
    rows_to_fill = old_nrows;
  } else {
    resptr += old_alloc_size;
  }
  for (const Column* col : columns) {
    if (col->stype() == SType::VOID) {
      rows_to_fill += col->nrows;
    } else {
      if (rows_to_fill) {
        set_value(resptr, naptr, sizeof(T), rows_to_fill);
        resptr += rows_to_fill * sizeof(T);
        rows_to_fill = 0;
      }
      if (col->stype() != stype()) {
        Column* newcol = col->cast(stype());
        delete col;
        col = newcol;
      }
      std::memcpy(resptr, col->data(), col->alloc_size());
      resptr += col->alloc_size();
    }
    delete col;
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

void PyObjectColumn::rbind_impl(
  std::vector<const Column*>& columns, size_t nnrows, bool col_empty)
{
  size_t old_nrows = nrows;
  size_t new_nrows = nnrows;

  // Reallocate the column's data buffer
  // `resize` fills all new elements with Py_None
  mbuf.resize(sizeof(PyObject*) * new_nrows);
  nrows = nnrows;

  // Copy the data
  PyObject** dest_data = static_cast<PyObject**>(mbuf.wptr());
  PyObject** dest_data0 = dest_data;
  if (!col_empty) {
    dest_data += old_nrows;
  }
  for (const Column* col : columns) {
    if (col->stype() == SType::VOID) {
      dest_data += col->nrows;
    } else {
      if (col->stype() != SType::OBJ) {
        Column* newcol = col->cast(stype());
        delete col;
        col = newcol;
      }
      auto src_data = static_cast<PyObject* const*>(col->data());
      for (size_t i = 0; i < col->nrows; ++i) {
        Py_INCREF(src_data[i]);
        Py_DECREF(*dest_data);
        *dest_data = src_data[i];
        dest_data++;
      }
    }
    delete col;
  }
  xassert(dest_data == dest_data0 + new_nrows);
  (void)dest_data0;
}


// Explicitly instantiate these templates:
template class FwColumn<int8_t>;
template class FwColumn<int16_t>;
template class FwColumn<int32_t>;
template class FwColumn<int64_t>;
template class FwColumn<float>;
template class FwColumn<double>;
template class FwColumn<PyObject*>;
template class StringColumn<uint32_t>;
template class StringColumn<uint64_t>;
