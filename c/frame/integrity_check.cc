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
#include "frame/py_frame.h"
#include "utils/exceptions.h"
#include "utils/misc.h"      // repr_utf8
#include "column_impl.h"
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
    if (stypes_tuple.size() != dt->ncols) {
      throw AssertionError() << "len(.stypes) = " << stypes_tuple.size()
          << " is different from .ncols = " << dt->ncols;
    }
    for (size_t i = 0; i < dt->ncols; ++i) {
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
    if (ltypes_tuple.size() != dt->ncols) {
      throw AssertionError() << "len(.ltypes) = " << ltypes_tuple.size()
          << " is different from .ncols = " << dt->ncols;
    }
    for (size_t i = 0; i < dt->ncols; ++i) {
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
  if (nkeys > ncols) {
    throw AssertionError()
        << "Number of keys is greater than the number of columns in the Frame: "
        << nkeys << " > " << ncols;
  }

  _integrity_check_names();
  _integrity_check_pynames();

  // Check the number of columns; the number of allocated columns should be
  // equal to `ncols`.
  if (columns.size() != ncols) {
    throw AssertionError()
        << "DataTable.columns array size is " << columns.size()
        << " whereas ncols = " << ncols;
  }
  if (names.size() != ncols) {
    throw AssertionError()
        << "Number of column names, " << names.size() << ", is not equal "
           "to the number of columns in the Frame: " << ncols;
  }

  /**
   * Check the structure and contents of the column array.
   *
   * DataTable's RowIndex and nrows are supposed to reflect the RowIndex and
   * nrows of each column, so we will just check that the datatable's values
   * are equal to those of each column.
   */
  for (size_t i = 0; i < ncols; ++i) {
    const std::string& col_name = names[i];
    const Column& col = columns[i];
    if (!col) {
      throw AssertionError() << col_name << " of Frame is null";
    }
    // Make sure the column and the datatable have the same value for `nrows`
    if (nrows != col.nrows()) {
      throw AssertionError()
          << "Mismatch in `nrows`: " << col_name << ".nrows = " << col.nrows()
          << ", while the Frame has nrows=" << nrows;
    }
    col->verify_integrity(col_name);
  }

  for (size_t i = 0; i < ncols; ++i) {
    const std::string& name = names[i];
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



//------------------------------------------------------------------------------
// Column
//------------------------------------------------------------------------------

void ColumnImpl::verify_integrity(const std::string&) const {
  mbuf.verify_integrity();

  // Check Stats
  if (stats) { // Stats are allowed to be null
    stats->verify_integrity();
  }
}



//------------------------------------------------------------------------------
// BoolColumn
//------------------------------------------------------------------------------

void BoolColumn::verify_integrity(const std::string& name) const {
  FwColumn<int8_t>::verify_integrity(name);

  // Check that all elements in column are either 0, 1, or NA_I1
  size_t mbuf_nrows = mbuf.size();
  const int8_t* vals = elements_r();
  for (size_t i = 0; i < mbuf_nrows; ++i) {
    int8_t val = vals[i];
    if (!(val == 0 || val == 1 || val == NA_I1)) {
      throw AssertionError()
          << "(Boolean) " << name << " has value " << val << " in row " << i;
    }
  }
}




//------------------------------------------------------------------------------
// StringColumn
//------------------------------------------------------------------------------

template <typename T>
void StringColumn<T>::verify_integrity(const std::string& name) const {
  ColumnImpl::verify_integrity(name);

  size_t strdata_size = 0;
  //*_utf8 functions use unsigned char*
  const uint8_t* cdata = reinterpret_cast<const uint8_t*>(strdata());
  const T* str_offsets = offsets();

  // Check that the offsets section is preceded by a -1
  if (str_offsets[-1] != 0) {
    throw AssertionError()
        << "Offsets section in (string) " << name << " does not start with 0";
  }

  size_t mbuf_nrows = mbuf.size()/sizeof(T) - 1;
  strdata_size = str_offsets[mbuf_nrows - 1] & ~GETNA<T>();

  // Check for the validity of each offset
  T lastoff = 0;
  for (size_t i = 0; i < mbuf_nrows; ++i) {
    T oj = str_offsets[i];
    if (ISNA<T>(oj)) {
      if (oj != (lastoff ^ GETNA<T>())) {
        throw AssertionError()
            << "Offset of NA String in row " << i << " of " << name
            << " does not have the same magnitude as the previous offset: "
               "offset = " << oj << ", previous offset = " << lastoff;
      }
    } else {
      if (oj < lastoff) {
        throw AssertionError()
            << "String offset in row " << i << " of " << name
            << " cannot be less than the previous offset: offset = " << oj
            << ", previous offset = " << lastoff;
      }
      if (!is_valid_utf8(cdata + lastoff, static_cast<size_t>(oj - lastoff))) {
        throw AssertionError()
            << "Invalid UTF-8 string in row " << i << " of " << name << ": "
            << repr_utf8(cdata + lastoff, cdata + oj);
      }
      lastoff = oj;
    }
  }
}



//------------------------------------------------------------------------------
// PyObjColumn
//------------------------------------------------------------------------------

void PyObjectColumn::verify_integrity(const std::string& name) const {
  FwColumn<py::robj>::verify_integrity(name);

  if (!mbuf.is_pyobjects()) {
    throw AssertionError() << "(object) " << name << "'s internal buffer is "
        "not marked as containing PyObjects";
  }

  // Check that all elements are valid pyobjects
  size_t mbuf_nrows = mbuf.size() / sizeof(PyObject*);
  const py::robj* vals = elements_r();
  for (size_t i = 0; i < mbuf_nrows; ++i) {
    py::robj val = vals[i];
    if (!val) {
      throw AssertionError() << "Object column " << name << " has NULL value "
          "in row " << i;
    }
    if (Py_REFCNT(val.to_borrowed_ref()) <= 0) {
      throw AssertionError()
          << "Element " << i << " in object column " << name
          << " has 0 refcount";
    }
  }
}


// Explicit instantiation of templates
template class StringColumn<uint32_t>;
template class StringColumn<uint64_t>;
