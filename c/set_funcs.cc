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
#include <tuple>
#include "datatablemodule.h"
#include "datatable.h"
#include "expr/py_expr.h"
#include "frame/py_frame.h"
#include "python/args.h"
#include "python/oiter.h"
#include "utils/assert.h"
#include "utils/exceptions.h"

namespace dt {
namespace set {

struct ccolvec {
  std::vector<const Column*> cols;
  std::string colname;
};

struct sort_result {
  std::vector<size_t> sizes;
  std::unique_ptr<Column> col;
  std::string colname;
  RowIndex ri;
  Groupby gb;
};


//------------------------------------------------------------------------------
// helper functions
//------------------------------------------------------------------------------

static void verify_frame_1column(DataTable* dt) {
  if (dt->ncols == 1) return;
  throw ValueError() << "Only single-column Frames are allowed, but received "
      "a Frame with " << dt->ncols << " columns";
}

static ccolvec columns_from_args(const py::PKArgs& args) {
  ccolvec res;
  for (auto va : args.varargs()) {
    if (va.is_frame()) {
      DataTable* dt = va.to_frame();
      if (dt->ncols == 0) continue;
      verify_frame_1column(dt);
      res.cols.push_back(dt->columns[0]->shallowcopy());
      if (res.colname.empty()) res.colname = dt->get_names()[0];
    }
    else if (va.is_iterable()) {
      for (auto item : va.to_pyiter()) {
        DataTable* dt = item.to_frame();
        if (dt->ncols == 0) continue;
        verify_frame_1column(dt);
        res.cols.push_back(dt->columns[0]->shallowcopy());
        if (res.colname.empty()) res.colname = dt->get_names()[0];
      }
    }
    else {
      throw TypeError() << args.get_short_name() << "() expects a list or "
          "sequence of Frames, but got an argument of type " << va.typeobj();
    }
  }
  return res;
}

static sort_result sort_columns(ccolvec& cv) {
  std::vector<const Column*>& cols = cv.cols;
  xassert(!cols.empty());
  sort_result res;
  res.colname = std::move(cv.colname);
  if (cols.size() == 1) {
    res.col = std::unique_ptr<Column>(const_cast<Column*>(cols[0]));
    res.col->reify();
  } else {
    // Note: `rbind` will delete all the columns in the vector `cols`...
    res.col = std::unique_ptr<Column>((new VoidColumn(0))->rbind(cols));
  }
  res.ri = res.col->sort(&res.gb);
  size_t cumsize = 0;
  for (auto col : cols) {
    cumsize += col->nrows;
    res.sizes.push_back(cumsize);
  }
  return res;
}

static Column* extract_uniques(const sort_result& sorted) {
  size_t ngrps = sorted.gb.ngroups();
  const int32_t* goffsets = sorted.gb.offsets_r();
  const int32_t* indices = sorted.ri.indices32();
  arr32_t arr(ngrps);
  int32_t* out_indices = arr.data();

  for (size_t i = 0; i < ngrps; ++i) {
    out_indices[i] = indices[goffsets[i]];
  }
  RowIndex out_ri = RowIndex::from_array32(std::move(arr), /* sorted= */ true);
  Column* outcol = sorted.col->shallowcopy(out_ri);
  outcol->reify();
  return outcol;
}



//------------------------------------------------------------------------------
// unique()
//------------------------------------------------------------------------------

static py::PKArgs fn_unique(
    1, 0, 0,        // Number of pos-only, pos/kw, and kw-only args
    false, false,   // varargs/varkws allowed?
    {"frame"},      // arg names
    "unique",       // function name
R"(unique(frame)
--

Find the unique values in the ``frame``.

The ``frame`` can have multiple columns, in which case the unique values from
all columns taken together will be returned.

This methods sorts the values in order to find the uniques. Thus, the return
values will be ordered. However, this should be considered an implementation
detail: in the future we may use a different algorithm (such as hash-based),
which may return the results in a different order.
)",

[](const py::PKArgs& args) -> py::oobj {
  DataTable* dt = args[0].to_frame();

  ccolvec cc;
  for (auto col : dt->columns) {
    cc.cols.push_back(col->shallowcopy());
  }
  if (dt->ncols == 1) {
    cc.colname = dt->get_names()[0];
  }
  sort_result s = sort_columns(cc);
  Column* col = extract_uniques(s);
  DataTable* newdt = new DataTable({col}, {s.colname});
  return py::oobj::from_new_reference(py::Frame::from_datatable(newdt));
});



//------------------------------------------------------------------------------
// union()
//------------------------------------------------------------------------------

static py::PKArgs fn_union(
    0, 0, 0,
    true, false,
    {},
    "union",
R"(union(*frames)
--

Find union of values in all `frames`.

Each frame should have only a single column (however, empty frames are allowed
too). The values in each frame will be treated as a set, and this function will
perform the Union operation on these sets. The result will be returned as a
single-column Frame. Input `frames` are allowed to have different stypes, in
which case they will be upcasted to the smallest common stype, similar to the
functionality of ``rbind()``.

This operation is equivalent to ``dt.unique(dt.rbind(*frames))``.
)",

[](const py::PKArgs& args) -> py::oobj {
  ccolvec cols = columns_from_args(args);
  sort_result s = sort_columns(cols);
  Column* col = extract_uniques(s);
  DataTable* dt = new DataTable({col}, {s.colname});
  return py::oobj::from_new_reference(py::Frame::from_datatable(dt));
});





} // namespace set
} // namespace dt


void DatatableModule::init_methods_sets() {
  ADDFN(dt::set::fn_unique);
  ADDFN(dt::set::fn_union);
}
