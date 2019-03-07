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
#include "frame/py_frame.h"
#include "utils/exceptions.h"
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
      SType col_stype = dt->columns[i]->stype();
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
      SType col_stype = dt->columns[i]->stype();
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

  /**
   * Check the structure and contents of the column array.
   *
   * DataTable's RowIndex and nrows are supposed to reflect the RowIndex and
   * nrows of each column, so we will just check that the datatable's values
   * are equal to those of each column.
   */
  for (size_t i = 0; i < ncols; ++i) {
    std::string col_name = std::string("Column ") + std::to_string(i);
    Column* col = columns[i];
    if (col == nullptr) {
      throw AssertionError() << col_name << " of Frame is null";
    }
    // Make sure the column and the datatable have the same value for `nrows`
    if (nrows != col->nrows) {
      throw AssertionError()
          << "Mismatch in `nrows`: " << col_name << ".nrows = " << col->nrows
          << ", while the Frame has nrows=" << nrows;
    }
    col->verify_integrity(col_name);
  }

  if (names.size() != ncols) {
    throw AssertionError()
        << "Number of column names, " << names.size() << ", is not equal "
           "to the number of columns in the Frame: " << ncols;
  }
  for (size_t i = 0; i < names.size(); ++i) {
    auto name = names[i];
    auto data = name.data();
    if (name.empty()) {
      throw AssertionError() << "Column " << i << " has empty name";
    }
    for (size_t j = 0; j < name.size(); ++j) {
      if (data[j] >= '\0' && data[j] < '\x20') {
        throw AssertionError() << "Column " << i << "'s name contains "
          "unprintable character " << data[j];
      }
    }
  }
}



//------------------------------------------------------------------------------
// Column
//------------------------------------------------------------------------------

void Column::verify_integrity(const std::string& name) const {
  mbuf.verify_integrity();
  ri.verify_integrity();

  size_t mbuf_nrows = data_nrows();

  // Check RowIndex
  if (ri.isabsent()) {
    // Check that nrows is a correct representation of mbuf's size
    if (nrows != mbuf_nrows) {
      throw AssertionError()
          << "Mismatch between reported number of rows: " << name
          << " has nrows=" << nrows << " but MemoryRange has data for "
          << mbuf_nrows << " rows";
    }
  }
  else {
    // Check that the length of the RowIndex corresponds to `nrows`
    if (nrows != ri.size()) {
      throw AssertionError()
          << "Mismatch in reported number of rows: " << name << " has "
          << "nrows=" << nrows << ", while its rowindex.length="
          << ri.size();
    }
    // Check that the maximum value of the RowIndex does not exceed the maximum
    // row number in the memory buffer
    if (ri.max() >= mbuf_nrows && ri.max() != RowIndex::NA) {
      throw AssertionError()
          << "Maximum row number in the rowindex of " << name << " exceeds the "
          << "number of rows in the underlying memory buffer: max(rowindex)="
          << ri.max() << ", and nrows(membuf)=" << mbuf_nrows;
    }
  }

  // Check Stats
  if (stats) { // Stats are allowed to be null
    stats->verify_integrity(this);
  }
}



//------------------------------------------------------------------------------
// BoolColumn
//------------------------------------------------------------------------------

void BoolColumn::verify_integrity(const std::string& name) const {
  FwColumn<int8_t>::verify_integrity(name);

  // Check that all elements in column are either 0, 1, or NA_I1
  size_t mbuf_nrows = data_nrows();
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
  Column::verify_integrity(name);

  size_t strdata_size = 0;
  //*_utf8 functions use unsigned char*
  const uint8_t* cdata = reinterpret_cast<const uint8_t*>(strdata());
  const T* str_offsets = offsets();

  // Check that the offsets section is preceded by a -1
  if (str_offsets[-1] != 0) {
    throw AssertionError()
        << "Offsets section in (string) " << name << " does not start with 0";
  }

  size_t mbuf_nrows = data_nrows();
  strdata_size = str_offsets[mbuf_nrows - 1] & ~GETNA<T>();

  if (strbuf.size() != strdata_size) {
    throw AssertionError()
        << "Size of string data section in " << name << " does not correspond"
           " to the magnitude of the final offset: size = " << strbuf.size()
        << ", expected " << strdata_size;
  }

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


// Explicit instantiation of templates
template class StringColumn<uint32_t>;
template class StringColumn<uint64_t>;
