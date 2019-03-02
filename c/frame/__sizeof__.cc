//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
//------------------------------------------------------------------------------
#include "frame/py_frame.h"
#include "python/_all.h"
namespace py {



//------------------------------------------------------------------------------
// Frame::__sizeof__
//------------------------------------------------------------------------------

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
  return oint(sz);
}


void Frame::Type::_init_sizeof(Methods& mm) {
  ADD_METHOD(mm, &Frame::m__sizeof__, args__sizeof__);
}



}  // namespace py

//------------------------------------------------------------------------------
// DataTable methods
//------------------------------------------------------------------------------

size_t DataTable::memory_footprint() const {
  size_t sz = 0;
  sz += sizeof(*this);
  sz += sizeof(Column*) * columns.capacity();
  sz += sizeof(std::string) * names.capacity();
  for (size_t i = 0; i < ncols; ++i) {
    sz += columns[i]->memory_footprint();
    sz += names[i].size();
  }
  if (py_names) {
    sz += py_names.get_sizeof();
    sz += py_inames.get_sizeof();
    for (size_t i = 0; i < ncols; ++i) {
      sz += py_names[i].get_sizeof();
    }
  }
  if (groupby) {
    sz += (groupby.ngroups() + 1) * sizeof(int32_t);
  }
  return sz;
}


/**
 * Get the total size of the memory occupied by the Column. This is different
 * from `column->alloc_size`, which in general reports byte size of the `data`
 * portion of the column.
 */
size_t Column::memory_footprint() const {
  size_t sz = sizeof(*this);
  if (!ri) sz += mbuf.memory_footprint();
  sz += ri.memory_footprint();
  if (stats) sz += stats->memory_footprint();
  return sz;
}


template <typename T>
size_t StringColumn<T>::memory_footprint() const {
  return Column::memory_footprint() + (ri? 0 : strbuf.memory_footprint());
}


template class StringColumn<uint32_t>;
template class StringColumn<uint64_t>;
