//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//------------------------------------------------------------------------------
#include "column/sentinel_fw.h"
#include "column/sentinel_str.h"
#include "frame/py_frame.h"
#include "utils/exceptions.h"
#include "utils/misc.h"      // repr_utf8
#include "datatable.h"
#include "encodings.h"


//------------------------------------------------------------------------------
// py::Frame
//------------------------------------------------------------------------------

void py::Frame::integrity_check() {
  if (!dt) {
    throw AssertionError() << "py::Frame.dt is NULL";
  }

  dt->verify_integrity();

  if (stypes) {
    if (!py::robj(stypes).is_tuple()) {
      throw AssertionError() << "py::Frame.stypes is not a tuple";
    }
    auto stypes_tuple = py::robj(stypes).to_otuple();
    if (stypes_tuple.size() != dt->ncols()) {
      throw AssertionError() << "len(.stypes) = " << stypes_tuple.size()
          << " is different from .ncols = " << dt->ncols();
    }
    for (size_t i = 0; i < dt->ncols(); ++i) {
      SType col_stype = dt->get_column(i).stype();
      auto elem = stypes_tuple[i];
      auto eexp = info(col_stype).py_stype();
      if (elem != eexp) {
        throw AssertionError() << "Element " << i << " of .stypes is "
            << elem << ", but the column's stype is " << col_stype;
      }
    }
  }
  if (ltypes) {
    if (!py::robj(ltypes).is_tuple()) {
      throw AssertionError() << "py::Frame.ltypes is not a tuple";
    }
    auto ltypes_tuple = py::robj(ltypes).to_otuple();
    if (ltypes_tuple.size() != dt->ncols()) {
      throw AssertionError() << "len(.ltypes) = " << ltypes_tuple.size()
          << " is different from .ncols = " << dt->ncols();
    }
    for (size_t i = 0; i < dt->ncols(); ++i) {
      SType col_stype = dt->get_column(i).stype();
      auto elem = ltypes_tuple[i];
      auto eexp = info(col_stype).py_ltype();
      if (elem != eexp) {
        throw AssertionError() << "Element " << i << " of .ltypes is "
            << elem << ", but the column's ltype is " << col_stype;
      }
    }
  }
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
      e = std::move(AssertionError() << "in column " << i
                    << " '" << col_name << "': " << e.to_string());
      throw;
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
  XAssert(static_cast<int64_t>(nrows_) >= 0);
  XAssert(static_cast<size_t>(stype_) < DT_STYPES_COUNT);
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
  assert_compatible_type<T>(stype_);
  XAssert(mbuf_.size() >= sizeof(T) * nrows_);
  mbuf_.verify_integrity();
}




//------------------------------------------------------------------------------
// SentinelBool_ColumnImpl
//------------------------------------------------------------------------------

void SentinelBool_ColumnImpl::verify_integrity() const {
  SentinelFw_ColumnImpl<int8_t>::verify_integrity();
  XAssert(stype_ == SType::BOOL);

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

  size_t strdata_size = 0;
  //*_utf8 functions use unsigned char*
  const uint8_t* cdata = static_cast<const uint8_t*>(strbuf_.rptr());
  const T* str_offsets = static_cast<const T*>(offbuf_.rptr()) + 1;

  // Check that the offsets section is preceded by a -1
  if (str_offsets[-1] != 0) {
    throw AssertionError()
        << "Offsets section in string column does not start with 0";
  }

  size_t mbuf_nrows = offbuf_.size()/sizeof(T) - 1;
  strdata_size = str_offsets[mbuf_nrows - 1] & ~GETNA<T>();

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
  SentinelFw_ColumnImpl<py::robj>::verify_integrity();

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
template class SentinelFw_ColumnImpl<py::robj>;
template class SentinelStr_ColumnImpl<uint32_t>;
template class SentinelStr_ColumnImpl<uint64_t>;

} // namespace dt
