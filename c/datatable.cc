//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "datatable.h"
#include <algorithm>
#include <limits>
#include "utils/parallel.h"
#include "py_utils.h"
#include "rowindex.h"
#include "types.h"



//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

DataTable::DataTable()
  : nrows(0), ncols(0), nkeys(0) {}


DataTable::DataTable(colvec&& cols) : DataTable()
{
  columns = std::move(cols);
  ncols = columns.size();
  if (ncols > 0) {
    nrows = columns[0]->nrows;
    rowindex = RowIndex(columns[0]->rowindex());

    bool need_to_materialize = false;
    for (size_t i = 1; i < ncols; ++i) {
      Column* col = columns[i];
      if (!col) throw ValueError() << "Column " << i << " is NULL";
      if (rowindex != col->rowindex()) {
        need_to_materialize = true;
      }
      if (col->nrows != nrows) {
        throw ValueError() << "Mismatched length in column " << i << ": "
                           << "found " << col->nrows << ", expected " << nrows;
      }
    }
    // TODO: remove in #1188
    if (need_to_materialize) {
      reify();
    }
  }
  set_names_to_default();
}


DataTable::DataTable(colvec&& cols, const py::olist& nn)
  : DataTable(std::move(cols))
{
  set_names(nn);
}

DataTable::DataTable(colvec&& cols, const strvec& nn)
  : DataTable(std::move(cols))
{
  set_names(nn);
}

DataTable::DataTable(colvec&& cols, const DataTable* nn)
  : DataTable(std::move(cols))
{
  copy_names_from(nn);
}

DataTable::~DataTable() {
  for (auto col : columns) delete col;
  columns.clear();
}




//------------------------------------------------------------------------------
// Public API
//------------------------------------------------------------------------------

/**
 * Make a shallow copy of the current DataTable.
 */
DataTable* DataTable::copy() const {
  colvec newcols;
  newcols.reserve(ncols);
  for (auto col : columns) {
    // Once Column becomes a proper class with copy-semantics, the `columns`
    // vector can be default-copied.
    newcols.push_back(col->shallowcopy());
  }
  DataTable* res = new DataTable(std::move(newcols), this);
  res->nkeys = nkeys;
  return res;
}



void DataTable::delete_columns(std::vector<size_t>& cols_to_remove) {
  if (cols_to_remove.empty()) return;
  std::sort(cols_to_remove.begin(), cols_to_remove.end());

  size_t next_col_to_remove = cols_to_remove[0];
  size_t j = 0;
  for (size_t i = 0, k = 0; i < ncols; ++i) {
    if (i == next_col_to_remove) {
      delete columns[i];
      // "delete" names[i];
      while (next_col_to_remove == i) {
        ++k;
        next_col_to_remove = k < cols_to_remove.size()
                             ? cols_to_remove[k]
                             : std::numeric_limits<size_t>::max();
      }
    } else {
      columns[j] = columns[i];
      std::swap(names[j], names[i]);
      ++j;
    }
  }
  // This may not be the same as `j` if there were repeating columns
  ncols = j;
  columns.resize(j);
  names.resize(j);
  py_names  = py::otuple();
  py_inames = py::odict();
}


void DataTable::delete_all() {
  for (size_t i = 0; i < ncols; ++i) {
    delete columns[i];
  }
  ncols = 0;
  nrows = 0;
  nkeys = 0;
  columns.resize(0);
  names.resize(0);
  py_names  = py::otuple();
  py_inames = py::odict();
}



// Split all columns into groups, by their `RowIndex`es
std::vector<RowColIndex> DataTable::split_columns_by_rowindices() const {
  std::vector<RowColIndex> res;
  for (size_t i = 0; i < ncols; ++i) {
    RowIndex r = columns[i]->rowindex();
    bool found = false;
    for (auto& item : res) {
      if (item.rowindex == r) {
        found = true;
        item.colindices.push_back(i);
        break;
      }
    }
    if (!found) {
      res.push_back({r, {i}});
    }
  }
  return res;
}


void DataTable::resize_rows(size_t new_nrows) {
  if (new_nrows == nrows) return;

  // Split all columns into groups, by their `RowIndex`es
  std::vector<RowIndex> rowindices;
  std::vector<std::vector<size_t>> colindices;
  for (size_t i = 0; i < ncols; ++i) {
    RowIndex r = columns[i]->remove_rowindex();
    size_t j = 0;
    for (; j < rowindices.size(); ++j) {
      if (rowindices[j] == r) break;
    }
    if (j == rowindices.size()) {
      rowindices.push_back(std::move(r));
      colindices.resize(j + 1);
    }
    colindices[j].push_back(i);
  }

  for (size_t j = 0; j < rowindices.size(); ++j) {
    RowIndex& r = rowindices[j];
    if (!r) r = RowIndex(size_t(0), nrows, size_t(1));
    r.resize(new_nrows);
    for (size_t i : colindices[j]) {
      columns[i]->replace_rowindex(r);
    }
  }
  nrows = new_nrows;
}



void DataTable::replace_rowindex(const RowIndex& newri) {
  if (!newri && !rowindex) return;
  rowindex = newri;
  nrows = rowindex.size();
  for (size_t i = 0; i < ncols; ++i) {
    columns[i]->replace_rowindex(rowindex);
  }
}


/**
 * Equivalent of ``return DT[ri, :]``.
 */
DataTable* apply_rowindex(const DataTable* dt, const RowIndex& ri) {
  auto rc = dt->split_columns_by_rowindices();
  colvec newcols(dt->ncols);
  for (auto& rcitem : rc) {
    RowIndex newri = ri * rcitem.rowindex;
    for (size_t i : rcitem.colindices) {
      newcols[i] = dt->columns[i]->shallowcopy(newri);
    }
  }
  return new DataTable(std::move(newcols), dt);
}


/**
 * Equivalent of ``DT = DT[ri, :]``.
 */
void DataTable::apply_rowindex(const RowIndex& ri) {
  // If RowIndex is empty, no need to do anything. Also, the expression
  // `ri.size()` cannot be computed.
  if (!ri) return;

  auto rc = split_columns_by_rowindices();
  for (auto& rcitem : rc) {
    RowIndex newri = ri * rcitem.rowindex;
    for (size_t i : rcitem.colindices) {
      columns[i]->replace_rowindex(newri);
    }
  }
  nrows = ri.size();
}


void DataTable::replace_groupby(const Groupby& newgb) {
  int32_t last_offset = newgb.offsets_r()[newgb.ngroups()];
  if (static_cast<size_t>(last_offset) != nrows) {
    throw ValueError() << "Cannot apply Groupby of " << last_offset << " rows "
      "to a Frame with " << nrows << " rows";
  }
  groupby = newgb;
}



/**
 * Convert a DataTable view into an actual DataTable. This is done in-place.
 * The resulting DataTable should have a NULL RowIndex and Stats array.
 * Do nothing if the DataTable is not a view.
 */
void DataTable::reify() {
  // if (rowindex.isabsent()) return;
  for (size_t i = 0; i < ncols; ++i) {
    columns[i]->reify();
  }
  rowindex.clear();
}



size_t DataTable::memory_footprint() const {
  size_t sz = 0;
  sz += sizeof(*this);
  sz += (ncols + 1) * sizeof(Column*);
  if (rowindex.isabsent()) {
    for (size_t i = 0; i < ncols; ++i) {
      sz += columns[i]->memory_footprint();
    }
  } else {
    // If table is a view, then ignore sizes of each individual column.
    sz += rowindex.memory_footprint();
  }
  return sz;
}



//------------------------------------------------------------------------------
// Compute stats
//------------------------------------------------------------------------------

DataTable* DataTable::_statdt(colmakerfn f) const {
  colvec out_cols;
  out_cols.reserve(ncols);
  for (auto col : columns) {
    out_cols.push_back((col->*f)());
  }
  return new DataTable(std::move(out_cols), this);
}

DataTable* DataTable::countna_datatable() const { return _statdt(&Column::countna_column); }
DataTable* DataTable::nunique_datatable() const { return _statdt(&Column::nunique_column); }
DataTable* DataTable::nmodal_datatable() const  { return _statdt(&Column::nmodal_column); }
DataTable* DataTable::mean_datatable() const    { return _statdt(&Column::mean_column); }
DataTable* DataTable::sd_datatable() const      { return _statdt(&Column::sd_column); }
DataTable* DataTable::skew_datatable() const    { return _statdt(&Column::skew_column); }
DataTable* DataTable::kurt_datatable() const    { return _statdt(&Column::kurt_column); }
DataTable* DataTable::min_datatable() const     { return _statdt(&Column::min_column); }
DataTable* DataTable::max_datatable() const     { return _statdt(&Column::max_column); }
DataTable* DataTable::mode_datatable() const    { return _statdt(&Column::mode_column); }
DataTable* DataTable::sum_datatable() const     { return _statdt(&Column::sum_column); }




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
