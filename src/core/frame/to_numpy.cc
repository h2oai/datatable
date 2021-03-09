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
#include "frame/py_frame.h"
#include "parallel/api.h"
#include "python/_all.h"
#include "python/args.h"
#include "python/string.h"
#include "datatablemodule.h"
#include "stype.h"
namespace py {


//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------
static oobj to_numpy_impl(oobj frame);

static bool datatable_has_nas(DataTable* dt) {
  for (size_t i = 0; i < dt->ncols(); ++i) {
    if (dt->get_column(i).na_count() > 0) {
      return true;
    }
  }
  return false;
}



//------------------------------------------------------------------------------
// to_numpy()
//------------------------------------------------------------------------------

static const char* doc_to_numpy =
R"(to_numpy(self, type=None, column=None)
--

Convert frame into a 2D numpy array, optionally forcing it into the
specified type.

In a limited set of circumstances the returned numpy array will be
created as a data view, avoiding copying the data. This happens if
all of these conditions are met:

  - the frame has only 1 column, which is not virtual;
  - the column's type is not string;
  - the `type` argument was not used.

In all other cases the returned numpy array will have a copy of the
frame's data. If the frame has multiple columns of different stypes,
then the values will be upcasted into the smallest common stype.

If the frame has any NA values, then the returned numpy array will
be an instance of `numpy.ma.masked_array`.

Parameters
----------
type: Type | <type-like>
    Cast frame into this type before converting it into a numpy
    array.

column: int
    Convert a single column from the frame.; the returned value will be
    a 1D-array instead of a regular 2D-array.

return: numpy.array
    The returned array will be 2-dimensional with the same :attr:`.shape`
    as the original frame. However, if the option `column` was used,
    then the returned array will be 1-dimensional with the length of
    :attr:`.nrows`.

except: ImportError
    If the ``numpy`` module is not installed.
)";

static PKArgs args_to_numpy(
    0, 2, 0, false, false,
    {"type", "column"}, "to_numpy", doc_to_numpy);


oobj Frame::to_numpy(const PKArgs& args) {
  const Arg& arg_type = args[0];
  const Arg& arg_column = args[1];

  dt::Type target_type = arg_type.to_type_force();
  if (arg_column.is_defined()) {
    auto i = arg_column.to_int64_strict();
    size_t icol = dt->xcolindex(i);
    Column col = dt->get_column(icol);
    if (target_type) {
      col.cast_inplace(target_type);
    }
    auto res = to_numpy_impl(
        Frame::oframe(new DataTable({col}, DataTable::default_names))
    );
    return res.invoke("reshape", {oint(col.nrows())});
  }
  else if (target_type) {
    colvec columns;
    columns.reserve(dt->ncols());
    for (size_t i = 0; i < dt->ncols(); i++) {
      columns.push_back(dt->get_column(i).cast(target_type));
    }
    return to_numpy_impl(Frame::oframe(new DataTable(std::move(columns), *dt)));
  }
  else {
    return to_numpy_impl(oobj(this));
  }
}


static oobj to_numpy_impl(oobj frame) {
  DataTable* dt = frame.to_datatable();
  oobj numpy = oobj::import("numpy");
  oobj nparray = numpy.get_attr("asfortranarray");

  size_t ncols = dt->ncols();
  dt::Type common_type;
  for (size_t i = 0; i < ncols; i++) {
    auto coltype = dt->get_column(i).type();
    common_type.promote(coltype);
    if (common_type.is_invalid()) {
      throw TypeError() << "Frame cannot be converted into a numpy array "
          "because it has columns of incompatible types";
    }
  }
  // date32 columns will be converted into int64 numpy arrays, and then
  // afterward we will "cast" that int64 array into datetime64[D].
  bool is_date32 = common_type.stype() == dt::SType::DATE32;
  if (is_date32) {
    auto target_type = dt::Type::int64();
    colvec columns;
    columns.reserve(dt->ncols());
    for (size_t i = 0; i < ncols; i++) {
      columns.push_back(dt->get_column(i).cast(target_type));
    }
    frame = Frame::oframe(new DataTable(std::move(columns), *dt));
  }

  oobj res;
  {
    getbuffer_exception = nullptr;
    // At this point, numpy will invoke py::Frame::m__getbuffer__
    res = nparray.call({frame});
    // If there was an exception in Frame::m__getbuffer__ then numpy will
    // "eat" it and create a 1x1 array containing the Frame object. In order
    // to prevent this, we check whether there was an exception in getbuffer,
    // and if so rethrow the exception.
    if (getbuffer_exception) {
      std::rethrow_exception(getbuffer_exception);
    }
  }
  if (is_date32) {
    auto np_date64_dtype = numpy.invoke("dtype", {py::ostring("datetime64[D]")});
    res = res.invoke("view", np_date64_dtype);
  }

  // If there are any columns with NAs, replace the numpy.array with
  // numpy.ma.masked_array
  if (datatable_has_nas(dt)) {
    size_t dtsize = ncols * dt->nrows();
    Column mask_col = Column::new_data_column(dtsize, dt::SType::BOOL);
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
        const Column& col = dt->get_column(icol);
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

static const char* doc_to_pandas =
R"(to_pandas(self)
--

Convert this frame to a pandas DataFrame.

Parameters
----------
return: pandas.DataFrame

except: ImportError
    If the `pandas` module is not installed.
)";

static PKArgs args_to_pandas(
    0, 0, 0, false, false, {}, "to_pandas", doc_to_pandas);


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
  args_to_numpy.add_synonym_arg("stype", "type");
  xt.add(METHOD(&Frame::to_numpy, args_to_numpy));
  xt.add(METHOD(&Frame::to_pandas, args_to_pandas));
}



}  // namespace py
