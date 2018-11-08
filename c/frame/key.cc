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
#include "python/list.h"



py::oobj py::Frame::get_key() const {
  py::otuple key(dt->nkeys);
  py::otuple names = get_names().to_pytuple();
  for (size_t i = 0; i < dt->nkeys; ++i) {
    key.set(i, names[i]);
  }
  return std::move(key);
}


void py::Frame::set_key(obj val) {
  if (val.is_none()) {
    dt->nkeys = 0;
    return;
  }
  std::vector<size_t> col_indices;
  if (val.is_string()) {
    size_t index = dt->xcolindex(val);
    col_indices.push_back(index);
  }
  else if (val.is_list_or_tuple()) {
    py::olist vallist = val.to_pylist();
    for (size_t i = 0; i < vallist.size(); ++i) {
      py::obj item = vallist[i];
      if (vallist[i].is_string()) {
        size_t index = dt->xcolindex(vallist[i]);
        col_indices.push_back(index);
      } else {
        throw TypeError() << "Key should be a list/tuple of column names, "
            "instead element " << i << " was a " << item.typeobj();
      }
    }
  }
  _clear_types();
  dt->set_keys(col_indices);
}



void DataTable::set_keys(std::vector<size_t>& col_indices) {
  if (col_indices.empty()) {
    nkeys = 0;
    return;
  }
  // Check that col_indices are unique
  size_t K = col_indices.size();
  for (size_t i = 0; i < K; ++i) {
    for (size_t j = i + 1; j < K; ++j) {
      if (col_indices[i] == col_indices[j]) {
        throw ValueError() << "Column `" << names[col_indices[i]] << "` is "
          "specified multiple times within the key";
      }
    }
  }
  // Fill col_indices with the indices of remaining columns
  auto is_key_column = [&](size_t i) -> bool {
    for (size_t j = 0; j < K; ++j) {
      if (col_indices[j] == i) return true;
    }
    return false;
  };
  for (size_t i = 0; i < ncols; ++i) {
    if (!is_key_column(i)) {
      col_indices.push_back(i);
    }
  }
  xassert(col_indices.size() == ncols);
  // Reorder the columns
  std::vector<Column*> new_columns(ncols, nullptr);
  for (size_t i = 0; i < ncols; ++i) {
    new_columns[i] = columns[col_indices[i]];
  }
  columns = std::move(new_columns);
  //
  reorder_names(col_indices);
  set_nkeys(K);
}
