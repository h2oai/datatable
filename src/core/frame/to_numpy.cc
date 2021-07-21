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
#include "datatablemodule.h"
#include "documentation.h"
#include "frame/py_frame.h"
#include "parallel/api.h"
#include "python/_all.h"
#include "python/args.h"
#include "python/string.h"
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

static PKArgs args_to_numpy(
    0, 2, 0, false, false,
    {"type", "column"}, "to_numpy", dt::doc_Frame_to_numpy);


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
  if (ncols == 0) {
    otuple shape(2);
    shape.set(0, oint(0));
    shape.set(1, oint(0));
    return numpy.invoke("empty", {shape});
  }

  dt::Type common_type;
  for (size_t i = 0; i < ncols; i++) {
    auto coltype = dt->get_column(i).type();
    common_type.promote(coltype);
    if (common_type.is_invalid()) {
      throw TypeError() << "Frame cannot be converted into a numpy array "
          "because it has columns of incompatible types";
    }
  }
  xassert(common_type);
  if (common_type.is_void()) {
    return numpy.invoke("full",
      {frame.get_attr("shape"), None(), ostring("float64")});
  }

  // date32 columns will be converted into int64 numpy arrays, and then
  // afterward we will "cast" that int64 array into datetime64[D]. We do not
  // want to use numpy's `.astype()` here, because our cast properly converts
  // INT32 NAs into INT64 NAs, which numpy then interprets as NaT values.
  bool is_date32 = common_type.stype() == dt::SType::DATE32;
  if (is_date32) {
    auto target_type = dt::Type::int64();
    colvec columns;
    columns.reserve(dt->ncols());
    for (size_t i = 0; i < ncols; i++) {
      columns.push_back(dt->get_column(i).cast(target_type));
    }
    // Note: `frame` is the owner of the `dt` pointer. First line creates a
    // new (unowned) DataTable object and stores the pointer in the `dt`
    // variable. The second line puts the new `dt` pointer into the `frame`
    // object, which will now be its owner. At the same time,
    // previous DataTable object owned by `frame` is now destroyed.
    dt = new DataTable(std::move(columns), DataTable::default_names);
    frame = Frame::oframe(dt);
  }

  // For time64 column no extra preparation is needed: it is already
  // isomorphic with int64 type. The only thing we'll do is to invoke
  // `.astype()` after the conversion.
  bool is_time64 = common_type.stype() == dt::SType::TIME64;

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
  if (is_time64) {
    auto np_time64_dtype = numpy.invoke("dtype", {py::ostring("datetime64[ns]")});
    res = res.invoke("view", np_time64_dtype);
  }

  // If there are any columns with NAs, replace the numpy.array with
  // numpy.ma.masked_array
  if (!(common_type.is_float() || common_type.is_temporal() || common_type.is_object() ||
        common_type.is_string())
      && datatable_has_nas(dt))
  {
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
// Declare Frame methods
//------------------------------------------------------------------------------

void Frame::_init_tonumpy(XTypeMaker& xt) {
  args_to_numpy.add_synonym_arg("stype", "type");
  xt.add(METHOD(&Frame::to_numpy, args_to_numpy));
}



}  // namespace py
