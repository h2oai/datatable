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
#include "frame/py_frame.h"
#include "parallel/api.h"
#include "python/_all.h"
#include "python/args.h"
#include "python/string.h"
#include "datatablemodule.h"
namespace py {


//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

static bool datatable_has_nas(DataTable* dt, size_t force_col) {
  if (force_col != size_t(-1)) {
    return dt->get_column(force_col).na_count() > 0;
  }
  for (size_t i = 0; i < dt->ncols(); ++i) {
    if (dt->get_column(i).na_count() > 0) {
      return true;
    }
  }
  return false;
}


class pybuffers_context {
  public:
    pybuffers_context(SType st, size_t col) {
      pybuffers::force_stype = st;
      pybuffers::single_col = col;
    }
    ~pybuffers_context() {
      pybuffers::force_stype = SType::VOID;
      pybuffers::single_col = size_t(-1);
    }
};



//------------------------------------------------------------------------------
// to_numpy()
//------------------------------------------------------------------------------


static PKArgs args_to_numpy(
    0, 2, 0, false, false,
    {"stype", "column"}, "to_numpy",

R"(to_numpy(self, stype=None)
--

Convert frame into a 2D numpy array, optionally forcing it into the
specified stype/dtype.

In a limited set of circumstances the returned numpy array will be
created as a data view, avoiding copying the data. This happens if
all of these conditions are met:

  - the frame is not a view;
  - the frame has only 1 column;
  - the column's type is not string;
  - the `stype` argument was not used.

In all other cases the returned numpy array will have a copy of the
frame's data. If the frame has multiple columns of different stypes,
then the values will be upcasted into the smallest common stype.

If the frame has any NA values, then the returned numpy array will
be an instance of `numpy.ma.masked_array`.

Parameters
----------
stype: datatable.stype, numpy.dtype or str
    Cast frame into this stype before converting it into a numpy
    array.

column: int
    Convert only the specified column; the returned value will be
    a 1D-array instead of a regular 2D-array.
)");


oobj Frame::to_numpy(const PKArgs& args) {
  oobj numpy = oobj::import("numpy");
  oobj nparray = numpy.get_attr("array");
  SType stype      = args.get<SType>(0, SType::VOID);
  size_t force_col = args.get<size_t>(1, size_t(-1));

  oobj res;
  {
    pybuffers_context ctx(stype, force_col);
    res = nparray.call({oobj(this)});
  }

  // If there are any columns with NAs, replace the numpy.array with
  // numpy.ma.masked_array
  if (datatable_has_nas(dt, force_col)) {
    bool one_col = force_col != size_t(-1);
    size_t ncols = one_col? 1 : dt->ncols();
    size_t i0 = one_col? force_col : 0;

    size_t dtsize = ncols * dt->nrows();
    Column mask_col = Column::new_data_column(dtsize, SType::BOOL);
    bool* mask_data = static_cast<bool*>(mask_col.get_data_editable());

    size_t n_row_chunks = std::max(dt->nrows() / 100, size_t(1));
    size_t rows_per_chunk = dt->nrows() / n_row_chunks;
    size_t n_chunks = ncols * n_row_chunks;
    // precompute `countna` for all columns
    for (size_t j = 0; j < ncols; ++j) dt->get_column(j).na_count();

    dt::parallel_for_static(n_chunks,
      [&](size_t j) {
        size_t icol = j / n_row_chunks;
        size_t irow = j - (icol * n_row_chunks);
        size_t row0 = irow * rows_per_chunk;
        size_t row1 = irow == n_row_chunks-1? dt->nrows() : row0 + rows_per_chunk;
        bool* mask_ptr = mask_data + icol * dt->nrows();
        const Column& col = dt->get_column(icol + i0);
        col.fill_npmask(mask_ptr, row0, row1);
      });

    DataTable* mask_dt = new DataTable({std::move(mask_col)},
                                       DataTable::default_names);
    oobj mask_frame = Frame::oframe(mask_dt);
    oobj mask_array = nparray.call({mask_frame});

    mask_array = mask_array.invoke("reshape", {oint(ncols), oint(dt->nrows())})
                 .get_attr("T");

    res = numpy.get_attr("ma").get_attr("masked_array")
          .call({res, mask_array});
  }

  return res;
}



//------------------------------------------------------------------------------
// to_pandas()
//------------------------------------------------------------------------------

static PKArgs args_to_pandas(
    0, 0, 0, false, false, {}, "to_pandas",

R"(to_pandas(self)
--

Convert this frame to a pandas DataFrame.

The `pandas` module is required to run this function.
)");


oobj Frame::to_pandas(const PKArgs&) {
  // ```
  // from pandas import DataFrame
  // names = self.names
  // ```
  oobj pandas = oobj::import("pandas");
  oobj dataframe = pandas.get_attr("DataFrame");
  otuple names = dt->get_pynames();

  // ```
  // cols = {names[i]: self.to_numpy(None, i)
  //         for i in range(self.ncols)}
  // ```
  odict cols;
  otuple np_call_args(2);
  np_call_args.set(0, py::None());
  for (size_t i = 0; i < dt->ncols(); ++i) {
    np_call_args.replace(1, oint(i));
    args_to_numpy.bind(np_call_args.to_borrowed_ref(), nullptr);
    cols.set(names[i],
             to_numpy(args_to_numpy));
  }
  // ```
  // return DataFrame(cols, columns=names)
  // ```
  odict pd_call_kws;
  pd_call_kws.set(ostring("columns"), names);
  return dataframe.call(otuple(cols), pd_call_kws);
}




//------------------------------------------------------------------------------
// Declare Frame methods
//------------------------------------------------------------------------------

void Frame::_init_tonumpy(XTypeMaker& xt) {
  xt.add(METHOD(&Frame::to_numpy, args_to_numpy));
  xt.add(METHOD(&Frame::to_pandas, args_to_pandas));
}



}  // namespace py
