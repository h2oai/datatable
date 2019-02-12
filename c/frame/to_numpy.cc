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
#include "python/_all.h"
#include "python/args.h"

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

static bool datatable_has_nas(DataTable* dt) {
  for (size_t i = 0; i < dt->ncols; ++i) {
    if (dt->columns[i]->countna() > 0) {
      return true;
    }
  }
  return false;
}



//------------------------------------------------------------------------------
// to_numpy()
//------------------------------------------------------------------------------
namespace py {


PKArgs Frame::Type::fn_to_numpy(
    0, 1, 0, false, false,
    {"stype"}, "to_numpy",
R"(to_numpy(self, stype=None)
--

Convert frame into a numpy array, optionally forcing it into the
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
    Cast frame into this stype before converting it into a numpy array.
)");


oobj Frame::to_numpy(const PKArgs& args) {
  oobj numpy = oobj::import("numpy");
  oobj nparray = numpy.get_attr("array");
  SType st = SType::VOID;
  if (!args[0].is_undefined()) {
    st = args[0].to_stype();
  }
  oobj res;
  otuple call_args1(1);
  call_args1.set(0, oobj(this));
  force_stype = st;
  res = nparray.call(call_args1);
  force_stype = SType::VOID;

  // If there are any columns with NAs, replace the numpy.array with
  // numpy.ma.masked_array
  if (datatable_has_nas(dt)) {
    size_t dtsize = dt->ncols * dt->nrows;
    Column* mask_col = Column::new_data_column(SType::BOOL, dtsize);
    int8_t* mask_data = static_cast<int8_t*>(mask_col->data_w());

    size_t n_row_chunks = std::max(dt->nrows / 100, size_t(1));
    size_t rows_per_chunk = dt->nrows / n_row_chunks;
    size_t n_chunks = dt->ncols * n_row_chunks;
    #pragma omp parallel for
    for (size_t j = 0; j < n_chunks; ++j) {
      size_t icol = j / n_row_chunks;
      size_t irow = j - (icol * n_row_chunks);
      size_t row0 = irow * rows_per_chunk;
      size_t row1 = irow == n_row_chunks-1? dt->nrows : row0 + rows_per_chunk;
      int8_t* mask_ptr = mask_data + icol * dt->nrows;
      Column* col = dt->columns[icol];
      if (col->countna()) {
        col->fill_na_mask(mask_ptr, row0, row1);
      } else {
        std::memset(mask_ptr, 0, row1 - row0);
      }
    }

    DataTable* mask_dt = new DataTable({mask_col});
    oobj mask_frame = oobj::from_new_reference(Frame::from_datatable(mask_dt));
    call_args1.set(0, mask_frame);
    oobj mask_array = nparray.call(call_args1);

    otuple call_args2(2);
    call_args2.set(0, oint(dt->ncols));
    call_args2.set(1, oint(dt->nrows));
    mask_array = mask_array.invoke("reshape", call_args2).get_attr("T");

    call_args2.set(0, res);
    call_args2.set(1, mask_array);
    res = numpy.get_attr("ma").get_attr("masked_array").call(call_args2);
  }

  return res;
}



}  // namespace py
