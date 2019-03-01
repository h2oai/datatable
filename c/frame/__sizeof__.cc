//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
//------------------------------------------------------------------------------
#include "frame/py_frame.h"
#include "python/_all.h"
namespace py {


static PKArgs args__sizeof__(
  0, 0, 0, false, false, {}, "__sizeof__",

R"(__sizeof__(self)
--

Return the size of this Frame in memory.

The function attempts to compute the total memory size of the Frame
as precisely as possible. In particular, it takes into account not
only the size of data in columns, but also sizes of all auxiliary
internal structures.

Special cases: if Frame is a view (say, `d2 = DT[:1000, :]`), then
the reported size will not contain the size of the data, because that
data "belongs" to the original datatable and is not copied. However if
a Frame selects only a subset of columns (say, `d3 = DT[:, :5]`),
then a view is not created and instead the columns are copied by
reference. Frame `d3` will report the "full" size of its columns,
even though they do not occupy any extra memory compared to `DT`.
This behavior may be changed in the future.

This function is not intended for manual use. Instead, in order to
get the size of a datatable `DT`, call `sys.getsizeof(DT)`.
)");


oobj Frame::m__sizeof__(const PKArgs&) {
  size_t sz = dt->memory_footprint();
  sz += sizeof(*this);
  if (ltypes) sz += _PySys_GetSizeOf(ltypes);
  if (stypes) sz += _PySys_GetSizeOf(stypes);

  /*
  // if (self->names) {
  //   PyObject* names = self->names;
  //   sz += _PySys_GetSizeOf(names);
  //   for (Py_ssize_t i = 0; i < Py_SIZE(names); ++i) {
  //     sz += _PySys_GetSizeOf(PyTuple_GET_ITEM(names, i));
  //   }
  // }
  */
  return oint(sz);
}


void Frame::Type::_init_sizeof(Methods& mm) {
  ADD_METHOD(mm, &Frame::m__sizeof__, args__sizeof__);
}


}
