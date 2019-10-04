//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#include <algorithm>
#include <limits>
#include "datatable.h"
#include "datatablemodule.h"
#include "rowindex.h"
#include "types.h"



//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

DataTable::DataTable()
  : nrows_(0), ncols_(0), nkeys_(0)
{
  TRACK(this, sizeof(*this), "DataTable");
}

DataTable::~DataTable() {
  UNTRACK(this);
}

// Private constructor, initializes only columns but not names_
DataTable::DataTable(colvec&& cols) : DataTable()
{
  if (cols.empty()) return;
  columns_ = std::move(cols);
  ncols_ = columns_.size();
  nrows_ = columns_[0].nrows();
  #if DTDEBUG
    for (const auto& col : columns_) {
      xassert(col && col.nrows() == nrows_);
    }
  #endif
}

DataTable::DataTable(colvec&& cols, DefaultNamesTag)
  : DataTable(std::move(cols))
{
  set_names_to_default();
}


DataTable::DataTable(colvec&& cols, const py::olist& nn, bool warn)
  : DataTable(std::move(cols))
{
  set_names(nn, warn);
}

DataTable::DataTable(colvec&& cols, const strvec& nn, bool warn)
  : DataTable(std::move(cols))
{
  set_names(nn, warn);
}

DataTable::DataTable(colvec&& cols, const DataTable& nn)
  : DataTable(std::move(cols))
{
  copy_names_from(nn);
}




//------------------------------------------------------------------------------
// Public API
//------------------------------------------------------------------------------

const Column& DataTable::get_column(size_t i) const {
  xassert(i < columns_.size());
  return columns_[i];
}

Column& DataTable::get_column(size_t i) {
  xassert(i < columns_.size());
  return columns_[i];
}

void DataTable::set_column(size_t i, Column&& newcol) {
  xassert(i < columns_.size());
  xassert(newcol.nrows() == nrows_);
  columns_[i] = std::move(newcol);
}


size_t DataTable::xcolindex(int64_t index) const {
  int64_t incols = static_cast<int64_t>(ncols_);
  if (index < -incols || index >= incols) {
    throw ValueError() << "Column index `" << index << "` is invalid "
        "for a frame with " << ncols_ << " column" << (ncols_ == 1? "" : "s");
  }
  if (index < 0) index += incols;
  xassert(index >= 0 && index < incols);
  return static_cast<size_t>(index);
}


/**
 * Make a shallow copy of the current DataTable.
 */
DataTable DataTable::copy() const {
  colvec newcols = columns_;  // copy the vector
  DataTable res(std::move(newcols), *this);
  res.nkeys_ = nkeys_;
  return res;
}


DataTable DataTable::extract_column(size_t i) const {
  xassert(i < ncols_);
  return DataTable({columns_[i]}, {names_[i]});
}


void DataTable::delete_columns(intvec& cols_to_remove) {
  if (cols_to_remove.empty()) return;
  std::sort(cols_to_remove.begin(), cols_to_remove.end());
  cols_to_remove.push_back(size_t(-1));  // guardian value

  size_t j = 0;
  for (size_t i = 0, k = 0; i < ncols_; ++i) {
    if (i == cols_to_remove[k]) {
      // cols_to_remove[] array may contain duplicate values of `i`, so we
      // skip them. This loop will always terminate, since the last entry
      // in `cols_to_remove` is -1, which cannot be equal to any `i`.
      while (i == cols_to_remove[k]) {
        k++;
      }
      continue;
    }
    if (i != j) {
      std::swap(columns_[j], columns_[i]);
      std::swap(names_[j], names_[i]);
    }
    ++j;
  }
  ncols_ = j;
  columns_.resize(j);
  names_.resize(j);
  py_names_  = py::otuple();
  py_inames_ = py::odict();
}


void DataTable::delete_all() {
  ncols_ = 0;
  nrows_ = 0;
  nkeys_ = 0;
  columns_.resize(0);
  names_.resize(0);
  py_names_  = py::otuple();
  py_inames_ = py::odict();
}


void DataTable::resize_rows(size_t new_nrows) {
  if (new_nrows == nrows_) return;
  if (new_nrows > nrows_ && nkeys_ > 0) {
    throw ValueError() << "Cannot increase the number of rows in a keyed frame";
  }
  for (Column& col : columns_) {
    col.resize(new_nrows);
  }
  nrows_ = new_nrows;
}


void DataTable::resize_columns(const strvec& new_names) {
  ncols_ = new_names.size();
  columns_.resize(ncols_);
  set_names(new_names);
}



/**
 * Equivalent of ``DT = DT[rowindex, :]``.
 */
void DataTable::apply_rowindex(const RowIndex& rowindex) {
  // If RowIndex is empty, no need to do anything. Also, the expression
  // `rowindex.size()` cannot be computed.
  if (!rowindex) return;
  for (Column& col : columns_) {
    col.apply_rowindex(rowindex);
  }
  nrows_ = rowindex.size();
}




/**
 * Materialize all columns in the DataTable.
 */
void DataTable::materialize() {
  for (Column& col : columns_) {
    col.materialize();
  }
}

