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
#include <numeric>
#include "frame/py_frame.h"
#include "python/list.h"
#include "sort.h"



//------------------------------------------------------------------------------
// py::Frame API
//------------------------------------------------------------------------------
namespace py {

static GSArgs args_key(
  "key",
R"(Tuple of column names that serve as a primary key for this Frame.

If the Frame is not keyed, this will return an empty tuple.

Assigning to this property will make the Frame keyed by the specified
column(s). The key columns will be moved to the front, and the Frame
will be sorted. The values in the key columns must be unique.
)");


oobj Frame::get_key() const {
  otuple key(dt->nkeys());
  otuple names = get_names().to_otuple();
  for (size_t i = 0; i < key.size(); ++i) {
    key.set(i, names[i]);
  }
  return std::move(key);
}


void Frame::set_key(const Arg& val) {
  if (val.is_none()) {
    return dt->clear_key();
  }
  std::vector<size_t> col_indices;
  if (val.is_string()) {
    size_t index = dt->xcolindex(robj(val));
    col_indices.push_back(index);
  }
  else if (val.is_list_or_tuple()) {
    olist vallist = val.to_pylist();
    for (size_t i = 0; i < vallist.size(); ++i) {
      robj item = vallist[i];
      if (vallist[i].is_string()) {
        size_t index = dt->xcolindex(vallist[i]);
        col_indices.push_back(index);
      } else {
        throw TypeError() << "Key should be a list/tuple of column names, "
            "instead element " << i << " was a " << item.typeobj();
      }
    }
  }
  else {
    throw TypeError() << "Key should be a column name, or a list/tuple of "
        "column names, instead it was a " << val.typeobj();
  }
  _clear_types();
  dt->set_key(col_indices);
}



void Frame::_init_key(XTypeMaker& xt) {
  xt.add(GETSET(&Frame::get_key, &Frame::set_key, args_key));
}



} // namespace py
//------------------------------------------------------------------------------
// DataTable API
//------------------------------------------------------------------------------

void DataTable::clear_key() {
  nkeys_ = 0;
}


void DataTable::set_key(std::vector<size_t>& col_indices) {
  if (col_indices.empty()) {
    nkeys_ = 0;
    return;
  }
  // Check that col_indices are unique
  size_t K = col_indices.size();
  for (size_t i = 0; i < K; ++i) {
    for (size_t j = i + 1; j < K; ++j) {
      if (col_indices[i] == col_indices[j]) {
        throw ValueError() << "Column `" << names_[col_indices[i]] << "` is "
          "specified multiple times within the key";
      }
    }
  }

  // Sort the table by the keys
  std::vector<Column> sort_cols;
  std::vector<SortFlag> sort_flags(K, SortFlag::NONE);
  for (size_t i : col_indices) {
    sort_cols.push_back(columns_[i]);
  }
  auto res = group(sort_cols, sort_flags);
  RowIndex ri = res.first;
  xassert(ri.size() == nrows_);
  // Note: it's possible to have ngroups > nrows, when grouping a 0-row Frame
  if (res.second.size() < nrows_) {
    throw ValueError() << "Cannot set a key: the values are not unique";
  }

  // Fill col_indices with the indices of remaining columns
  auto is_key_column = [&](size_t i) -> bool {
    for (size_t j = 0; j < K; ++j) {
      if (col_indices[j] == i) return true;
    }
    return false;
  };
  for (size_t i = 0; i < ncols_; ++i) {
    if (!is_key_column(i)) {
      col_indices.push_back(i);
    }
  }
  xassert(col_indices.size() == ncols_);

  // Reorder the columns
  colvec new_columns;
  new_columns.reserve(ncols_);
  for (size_t i = 0; i < ncols_; ++i) {
    Column col = columns_[col_indices[i]];  // copy
    col.apply_rowindex(ri);  // apply sort key
    new_columns.emplace_back(std::move(col));
  }
  columns_ = std::move(new_columns);
  reorder_names(col_indices);

  materialize(false);

  nkeys_ = K;
}


void DataTable::set_nkeys_unsafe(size_t K) {
  nkeys_ = K;
}
