//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
//------------------------------------------------------------------------------
#include "column/sentinel_str.h"
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


void Frame::_init_sizeof(XTypeMaker& xt) {
  xt.add(METHOD(&Frame::m__sizeof__, args__sizeof__));
}



}  // namespace py

//------------------------------------------------------------------------------
// DataTable methods
//------------------------------------------------------------------------------

size_t DataTable::memory_footprint() const noexcept {
  size_t sz = 0;
  sz += sizeof(*this);
  sz += sizeof(Column) * columns_.capacity();
  sz += sizeof(std::string) * names_.capacity();
  for (size_t i = 0; i < ncols_; ++i) {
    sz += columns_[i].memory_footprint();
    sz += names_[i].size();
  }
  if (py_names_) {
    sz += py_names_.get_sizeof();
    sz += py_inames_.get_sizeof();
    for (size_t i = 0; i < ncols_; ++i) {
      sz += py_names_[i].get_sizeof();
    }
  }
  return sz;
}


template <typename T>
size_t dt::SentinelStr_ColumnImpl<T>::memory_footprint() const noexcept {
  return sizeof(*this)
         + offbuf_.memory_footprint()
         + strbuf_.memory_footprint()
         + (stats_? stats_->memory_footprint() : 0);
}


template class dt::SentinelStr_ColumnImpl<uint32_t>;
template class dt::SentinelStr_ColumnImpl<uint64_t>;
