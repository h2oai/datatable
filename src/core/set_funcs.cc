//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include <functional>
#include <tuple>
#include "frame/py_frame.h"
#include "python/_all.h"
#include "python/xargs.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "datatable.h"
#include "documentation.h"
#include "sort.h"
#include "stype.h"
namespace dt {
namespace set {


struct named_colvec {
  colvec columns;
  std::string name;
};

struct sort_result {
  sztvec sizes;
  Column column;
  std::string colname;
  RowIndex ri;
  Groupby gb;
};


//------------------------------------------------------------------------------
// helper functions
//------------------------------------------------------------------------------

static py::oobj make_pyframe(sort_result&& sorted, Buffer&& buf) {
  // The array of rowindices `arr` is typically shuffled because the values
  // in the input are sorted before they are compared.
  RowIndex out_ri = RowIndex(std::move(buf), RowIndex::ARR32);
  sorted.column.apply_rowindex(out_ri);
  DataTable* dt = new DataTable({std::move(sorted.column)},
                                {std::move(sorted.colname)});
  return py::Frame::oframe(dt);
}

static py::oobj make_empty(sort_result&& sorted) {
    return py::Frame::oframe(new DataTable(
        {Column::new_na_column(0, sorted.column.stype())},
        {std::move(sorted.colname)}
    ));
}


static named_colvec columns_from_args(const py::XArgs& args) {
  using FN = std::function<void(py::robj,size_t)>;
  named_colvec result;

  FN process_arg = [&](py::robj arg, size_t level) {
    if (arg.is_frame()) {
      DataTable* dt = arg.to_datatable();
      if (dt->ncols() == 0) return;
      if (dt->ncols() > 1) {
        throw ValueError() << "Only single-column Frames are allowed, "
            "but received a Frame with " << dt->ncols() << " columns";
      }
      Column col = dt->get_column(0);  // copy
      col.materialize();
      result.columns.push_back(std::move(col));
      if (result.name.empty()) {
        result.name = dt->get_names()[0];
      }
    }
    else if (arg.is_iterable() && !arg.is_string() && level < 2) {
      for (auto item : arg.to_oiter()) {
        process_arg(item, level+1);
      }
    }
    else {
      throw TypeError() << args.proper_name() << "() expects a list or "
          "sequence of Frames, but got an argument of type " << arg.typeobj();
    }
  };

  for (auto va : args.varargs()) {
    process_arg(va, 0);
  }
  return result;
}


static sort_result sort_columns(named_colvec&& ncv) {
  xassert(!ncv.columns.empty());
  sort_result res;
  res.colname = std::move(ncv.name);
  size_t cumsize = 0;
  for (const auto& col : ncv.columns) {
    cumsize += col.nrows();
    res.sizes.push_back(cumsize);
  }
  if (ncv.columns.size() == 1) {
    res.column = std::move(ncv.columns[0]);
    res.column.materialize();
  } else {
    res.column = Column::new_na_column(0, SType::VOID);
    res.column.rbind(ncv.columns, false);
  }
  auto r = group({res.column}, {SortFlag::NONE});
  res.ri = std::move(r.first);
  res.gb = std::move(r.second);
  return res;
}


static py::oobj _union(named_colvec&& ncv) {
  if (ncv.columns.empty()) {
    return py::Frame::oframe(new DataTable());
  }
  sort_result sorted = sort_columns(std::move(ncv));

  size_t numGroups = sorted.gb.size();
  const int32_t* groupOffsets = sorted.gb.offsets_r();
  xassert(groupOffsets != nullptr);
  if (groupOffsets[numGroups] == 0) {
    return make_empty(std::move(sorted));
  }
  if (numGroups == 1) {
    size_t index;
    bool valid = sorted.ri.get_element(0, &index);
    if (valid) {
      xassert(index == 0);
      sorted.column.resize(1);
      return py::Frame::oframe(new DataTable(
          {std::move(sorted.column)},
          {std::move(sorted.colname)}
      ));
    } else {
      return py::Frame::oframe(new DataTable(
          {Column::new_na_column(1, sorted.column.stype())},
          {std::move(sorted.colname)}
      ));
    }
  }
  if (sorted.ri.isarr32()) {
    const int32_t* indices = sorted.ri.indices32();
    Buffer buf = Buffer::mem(numGroups * sizeof(int32_t));
    int32_t* out_indices = static_cast<int32_t*>(buf.xptr());

    for (size_t i = 0; i < numGroups; ++i) {
      out_indices[i] = indices[groupOffsets[i]];
    }
    return make_pyframe(std::move(sorted), std::move(buf));
  }
  throw NotImplError() << "Unexpected RowIndex type "
      << static_cast<int>(sorted.ri.type()) << " in _union()";
}




//------------------------------------------------------------------------------
// unique()
//------------------------------------------------------------------------------

static py::oobj unique(const py::XArgs& args) {
  if (!args[0]) {
    throw ValueError() << "Function `unique()` expects a Frame as a parameter";
  }
  DataTable* dt = args[0].to_datatable();

  named_colvec ncv;
  for (size_t i = 0; i < dt->ncols(); ++i) {
    ncv.columns.push_back(dt->get_column(i));
  }
  if (dt->ncols() == 1) {
    ncv.name = dt->get_names()[0];
  }
  return _union(std::move(ncv));
}

DECLARE_PYFN(&unique)
    ->name("unique")
    ->docs(doc_dt_unique)
    ->n_positional_args(1)
    ->n_required_args(1)
    ->arg_names({"frame"});




//------------------------------------------------------------------------------
// union()
//------------------------------------------------------------------------------

static py::oobj union_(const py::XArgs& args) {
  named_colvec cc = columns_from_args(args);
  return _union(std::move(cc));
}

DECLARE_PYFN(&union_)
    ->name("union")
    ->docs(doc_dt_union)
    ->allow_varargs();




//------------------------------------------------------------------------------
// intersect()
//------------------------------------------------------------------------------

template <bool TWO>
static py::oobj _intersect(named_colvec&& cv) {
  size_t K = cv.columns.size();
  sort_result sorted = sort_columns(std::move(cv));
  size_t ngrps = sorted.gb.size();
  const int32_t* goffsets = sorted.gb.offsets_r();
  if (goffsets[ngrps] == 0) {
    return make_empty(std::move(sorted));
  }

  const int32_t* indices;
  Buffer indicesBuffer;
  if (sorted.ri.isarr32()) {
    indices = sorted.ri.indices32();
  } else {
    sorted.ri.extract_into(indicesBuffer, RowIndex::ARR32);
    indices = static_cast<const int32_t*>(indicesBuffer.rptr());
  }
  Buffer buffer = Buffer::mem(ngrps * sizeof(int32_t));
  int32_t* out_indices = static_cast<int32_t*>(buffer.xptr());
  size_t j = 0;

  if (TWO) {
    // When intersecting only 2 vectors, it is enough to check whether the
    // first element in a group is < n1 (belongs to column 0), and the last is
    // >= n1 (belongs to column 1).
    xassert(K == 2);
    int32_t n1 = static_cast<int32_t>(sorted.sizes[0]);
    for (size_t i = 0; i < ngrps; ++i) {
      int32_t x = indices[goffsets[i]];
      if (x < n1) {
        int32_t y = indices[goffsets[i + 1] - 1];
        if (y >= n1) {
          out_indices[j++] = x;
        }
      }
    }
  } else {
    // When intersecting 3+ vectors, we scan the rowindex by group, and
    // for each element in a group we check whether it belongs to each
    // of the K input vectors.
    xassert(K > 2);
    int32_t iK = static_cast<int32_t>(K);
    int32_t off0, off1 = 0;
    for (size_t i = 1; i <= ngrps; ++i) {
      off0 = off1;
      off1 = goffsets[i];
      int32_t ii = off0;
      if (off0 + iK > off1) continue;
      for (size_t k = 0; k < K; ++k) {
        int32_t nk = static_cast<int32_t>(sorted.sizes[k]);
        if (indices[ii] >= nk) goto cont_outer_loop;
        while (ii < off1 && indices[ii] < nk) ++ii;
        if (ii == off1) {
          if (k == K - 1) {
            out_indices[j++] = indices[off0];
          }
          break;
        }
      }
      cont_outer_loop:;
    }
  }
  buffer.resize(j * sizeof(int32_t));
  return make_pyframe(std::move(sorted), std::move(buffer));
}

static py::oobj intersect(const py::XArgs& args) {
  named_colvec cv = columns_from_args(args);
  if (cv.columns.size() <= 1) {
    return _union(std::move(cv));
  }
  if (cv.columns.size() == 2) {
    return _intersect<true>(std::move(cv));
  } else {
    return _intersect<false>(std::move(cv));
  }
}

DECLARE_PYFN(&intersect)
    ->name("intersect")
    ->docs(doc_dt_intersect)
    ->allow_varargs();




//------------------------------------------------------------------------------
// setdiff()
//------------------------------------------------------------------------------

static py::oobj _setdiff(named_colvec&& cv) {
  xassert(cv.columns.size() >= 2);
  sort_result sorted = sort_columns(std::move(cv));
  size_t ngrps = sorted.gb.size();
  const int32_t* goffsets = sorted.gb.offsets_r();
  if (goffsets[ngrps] == 0) {
    return make_empty(std::move(sorted));
  }

  const int32_t* indices;
  Buffer indicesBuffer;
  if (sorted.ri.isarr32()) {
    indices = sorted.ri.indices32();
  } else {
    sorted.ri.extract_into(indicesBuffer, RowIndex::ARR32);
    indices = static_cast<const int32_t*>(indicesBuffer.rptr());
  }
  Buffer buffer = Buffer::mem(ngrps * sizeof(int32_t));
  int32_t* out_indices = static_cast<int32_t*>(buffer.xptr());
  size_t j = 0;

  int32_t n1 = static_cast<int32_t>(sorted.sizes[0]);
  for (size_t i = 0; i < ngrps; ++i) {
    int32_t x = indices[goffsets[i]];
    int32_t y = indices[goffsets[i + 1] - 1];
    if (x < n1 && y < n1) {
      out_indices[j++] = x;
    }
  }
  buffer.resize(j * sizeof(int32_t));
  return make_pyframe(std::move(sorted), std::move(buffer));
}

static py::oobj setdiff(const py::XArgs& args) {
  named_colvec cv = columns_from_args(args);
  if (cv.columns.size() <= 1) {
    return _union(std::move(cv));
  }
  return _setdiff(std::move(cv));
}

DECLARE_PYFN(&setdiff)
    ->name("setdiff")
    ->docs(doc_dt_setdiff)
    ->allow_varargs();




//------------------------------------------------------------------------------
// symdiff()
//------------------------------------------------------------------------------

template <bool TWO>
static py::oobj _symdiff(named_colvec&& cv) {
  size_t K = cv.columns.size();
  sort_result sr = sort_columns(std::move(cv));
  size_t ngrps = sr.gb.size();
  const int32_t* goffsets = sr.gb.offsets_r();
  if (goffsets[ngrps] == 0) {
    return make_empty(std::move(sr));
  }

  const int32_t* indices;
  Buffer indicesBuffer;
  if (sr.ri.isarr32()) {
    indices = sr.ri.indices32();
  } else {
    sr.ri.extract_into(indicesBuffer, RowIndex::ARR32);
    indices = static_cast<const int32_t*>(indicesBuffer.rptr());
  }
  Buffer buffer = Buffer::mem(ngrps * sizeof(int32_t));
  int32_t* out_indices = static_cast<int32_t*>(buffer.xptr());
  size_t j = 0;

  if (TWO) {
    // When sym-diffing only 2 vectors, it is enough to check whether the
    // first element in a group belongs to the same column as the last element
    // in a group
    xassert(K == 2);
    int32_t n1 = static_cast<int32_t>(sr.sizes[0]);
    for (size_t i = 0; i < ngrps; ++i) {
      int32_t x = indices[goffsets[i]];
      int32_t y = indices[goffsets[i + 1] - 1];
      if ((x < n1) == (y < n1)) {
        out_indices[j++] = x;
      }
    }
  } else {
    // When sym-diffing 3+ vectors, we scan the rowindex by group, and
    // for each group count the number of columns that have elements in
    // that group.
    xassert(K > 2);
    int32_t off0, off1 = 0;
    for (size_t i = 1; i <= ngrps; ++i) {
      off0 = off1;
      off1 = goffsets[i];
      int32_t ii = off0;
      size_t kk = 0;  // number of columns whose elements are in this group
      for (size_t k = 0; k < K; ++k) {
        int32_t nk = static_cast<int32_t>(sr.sizes[k]);
        if (indices[ii] >= nk) continue;
        kk++;
        while (ii < off1 && indices[ii] < nk) ++ii;
        if (ii == off1) break;
      }
      if (kk & 1) {
        out_indices[j++] = indices[off0];
      }
    }
  }
  buffer.resize(j * sizeof(int32_t));
  return make_pyframe(std::move(sr), std::move(buffer));
}

static py::oobj symdiff(const py::XArgs& args) {
  named_colvec cv = columns_from_args(args);
  if (cv.columns.size() <= 1) {
    return _union(std::move(cv));
  }
  if (cv.columns.size() == 2) {
    return _symdiff<true>(std::move(cv));
  } else {
    return _symdiff<false>(std::move(cv));
  }
}

DECLARE_PYFN(&symdiff)
    ->name("symdiff")
    ->docs(doc_dt_symdiff)
    ->allow_varargs();




}} // namespace dt::set
