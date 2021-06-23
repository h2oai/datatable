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
#include "column/sentinel_fw.h"
#include "column/sentinel_str.h"
#include "datatable.h"
#include "encodings.h"
#include "frame/py_frame.h"
#include "ltype.h"
#include "stype.h"
#include "utils/exceptions.h"
#include "utils/misc.h"      // repr_utf8


//------------------------------------------------------------------------------
// py::Frame
//------------------------------------------------------------------------------

void py::Frame::integrity_check() {
  if (!dt) {
    throw AssertionError() << "py::Frame.dt is NULL";
  }
  dt->verify_integrity();
}



//------------------------------------------------------------------------------
// DataTable
//------------------------------------------------------------------------------

/**
 * Verify that all internal constraints in the DataTable hold, and that there
 * are no any inappropriate values/elements.
 */
void DataTable::verify_integrity() const
{
  XAssert(nkeys_ <= ncols_);
  XAssert(columns_.size() == ncols_);
  XAssert(names_.size() == ncols_);

  _integrity_check_names();
  _integrity_check_pynames();


  /**
   * Check the structure and contents of the column array.
   *
   * DataTable's RowIndex and nrows are supposed to reflect the RowIndex and
   * nrows of each column, so we will just check that the datatable's values
   * are equal to those of each column.
   */
  for (size_t i = 0; i < ncols_; ++i) {
    const std::string& col_name = names_[i];
    const Column& col = columns_[i];
    if (!col) {
      throw AssertionError() << col_name << " of Frame is empty";
    }
    // Make sure the column and the datatable have the same value for `nrows`
    if (nrows_ != col.nrows()) {
      throw AssertionError()
          << "Mismatch in `nrows`: " << col_name << ".nrows = " << col.nrows()
          << ", while the Frame has nrows=" << nrows_;
    }
    try {
      col.verify_integrity();
    }
    catch (Error& e) {
      throw AssertionError() << "in column " << i
                << " '" << col_name << "': " << e.to_string();
    }
  }

  for (size_t i = 0; i < ncols_; ++i) {
    const std::string& name = names_[i];
    if (name.empty()) {
      throw AssertionError() << "Column " << i << " has empty name";
    }
    for (size_t j = 0; j < name.size(); ++j) {
      if (name[j] >= '\0' && name[j] < '\x20') {
        throw AssertionError() << "Column " << i << "'s name contains "
          "unprintable character " << name[j];
      }
    }
  }
}




namespace dt {

//------------------------------------------------------------------------------
// ColumnImpl
//------------------------------------------------------------------------------

void ColumnImpl::verify_integrity() const {
  XAssert(static_cast<int64_t>(nrows()) >= 0);
  XAssert(static_cast<size_t>(stype()) < dt::STYPES_COUNT);
  XAssert(refcount_ > 0 && refcount_ < uint32_t(-100));
  if (stats_) { // Stats are allowed to be null
    stats_->verify_integrity(this);
  }
}




//------------------------------------------------------------------------------
// SentinelFw_ColumnImpl<T>
//------------------------------------------------------------------------------

template <typename T>
void SentinelFw_ColumnImpl<T>::verify_integrity() const {
  ColumnImpl::verify_integrity();
  xassert(type_.can_be_read_as<T>());
  XAssert(mbuf_.size() >= sizeof(T) * nrows_);
  mbuf_.verify_integrity();
}




//------------------------------------------------------------------------------
// SentinelBool_ColumnImpl
//------------------------------------------------------------------------------

void SentinelBool_ColumnImpl::verify_integrity() const {
  SentinelFw_ColumnImpl<int8_t>::verify_integrity();
  XAssert(stype() == dt::SType::BOOL);

  // Check that all elements in column are either 0, 1, or NA_I1
  size_t mbuf_nrows = mbuf_.size();
  const int8_t* vals = static_cast<const int8_t*>(mbuf_.rptr());
  for (size_t i = 0; i < mbuf_nrows; ++i) {
    int8_t val = vals[i];
    if (!(val == 0 || val == 1 || val == NA_I1)) {
      throw AssertionError()
          << "boolean column has value " << val << " in row " << i;
    }
  }
}




//------------------------------------------------------------------------------
// SentinelStr_ColumnImpl
//------------------------------------------------------------------------------

template <typename T>
void SentinelStr_ColumnImpl<T>::verify_integrity() const {
  Sentinel_ColumnImpl::verify_integrity();
  offbuf_.verify_integrity();
  strbuf_.verify_integrity();

  //*_utf8 functions use unsigned char*
  const uint8_t* cdata = static_cast<const uint8_t*>(strbuf_.rptr());
  const T* str_offsets = static_cast<const T*>(offbuf_.rptr()) + 1;
  size_t mbuf_nrows = offbuf_.size()/sizeof(T) - 1;
  size_t strdata_size = str_offsets[mbuf_nrows - 1] & ~GETNA<T>();

  // Check that the buffer size is consistent with the offsets
  if (strbuf_.size() < strdata_size) {
    throw AssertionError()
        << "Size of the buffer `" << strbuf_.size() << "` is smaller than "
        << "the data size calculated from the offsets `" << strdata_size << "`";
  }

  // Check that the offsets section is preceded by a -1
  if (str_offsets[-1] != 0) {
    throw AssertionError()
        << "Offsets section in string column does not start with 0";
  }

  // Check for the validity of each offset
  T lastoff = 0;
  for (size_t i = 0; i < mbuf_nrows; ++i) {
    T oj = str_offsets[i];
    if (ISNA<T>(oj)) {
      if (oj != (lastoff ^ GETNA<T>())) {
        throw AssertionError()
            << "Offset of NA String in row " << i << " does not have the same "
               "magnitude as the previous offset: offset = "
            << oj << ", previous offset = " << lastoff;
      }
    } else {
      if (oj < lastoff) {
        throw AssertionError()
            << "String offset in row " << i << " cannot be less than the "
               "previous offset: offset = " << oj
            << ", previous offset = " << lastoff;
      }
      if (oj == lastoff) continue;
      XAssert(cdata);
      if (!is_valid_utf8(cdata + lastoff, static_cast<size_t>(oj - lastoff))) {
        throw AssertionError()
            << "Invalid UTF-8 string in row " << i << ": "
            << repr_utf8(cdata + lastoff, cdata + oj);
      }
      lastoff = oj;
    }
  }
}



//------------------------------------------------------------------------------
// PyObjColumn
//------------------------------------------------------------------------------

void SentinelObj_ColumnImpl::verify_integrity() const {
  SentinelFw_ColumnImpl<py::oobj>::verify_integrity();

  if (!mbuf_.is_pyobjects()) {
    throw AssertionError() << "obj64 column's internal buffer is "
        "not marked as containing PyObjects";
  }

  // Check that all elements are valid pyobjects
  size_t mbuf_nrows = mbuf_.size() / sizeof(PyObject*);
  const py::robj* vals = static_cast<const py::robj*>(mbuf_.rptr());
  for (size_t i = 0; i < mbuf_nrows; ++i) {
    py::robj val = vals[i];
    if (!val) {
      throw AssertionError() << "obj64 column has NULL value in row " << i;
    }
    if (Py_REFCNT(val.to_borrowed_ref()) <= 0) {
      throw AssertionError()
          << "Element " << i << " in obj64 column has refcount <= 0";
    }
  }
}


// Explicit instantiation of templates
template class SentinelFw_ColumnImpl<int8_t>;
template class SentinelFw_ColumnImpl<int16_t>;
template class SentinelFw_ColumnImpl<int32_t>;
template class SentinelFw_ColumnImpl<int64_t>;
template class SentinelFw_ColumnImpl<float>;
template class SentinelFw_ColumnImpl<double>;
template class SentinelFw_ColumnImpl<py::oobj>;
template class SentinelStr_ColumnImpl<uint32_t>;
template class SentinelStr_ColumnImpl<uint64_t>;

} // namespace dt
