//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "datatable.h"
#include <stdlib.h>
#include "utils/parallel.h"
#include "py_utils.h"
#include "rowindex.h"
#include "types.h"

// Forward declarations
using strvec = std::vector<std::string>;


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

DataTable::DataTable()
  : nrows(0), ncols(0), nkeys(0), py_inames(nullptr) {}


DataTable::DataTable(colvec&& cols) : DataTable()
{
  columns = std::move(cols);
  ncols = columns.size();
  if (ncols > 0) {
    nrows = static_cast<size_t>(columns[0]->nrows);
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
  return new DataTable(std::move(newcols), this);
}



DataTable* DataTable::delete_columns(int *cols_to_remove, int64_t n)
{
  if (n == 0) return this;
  qsort(cols_to_remove, static_cast<size_t>(n), sizeof(int),
        [](const void *a, const void *b) {
          const int x = *static_cast<const int*>(a);
          const int y = *static_cast<const int*>(b);
          return (x > y) - (x < y);
        });
  int j = 0;
  int next_col_to_remove = cols_to_remove[0];
  int64_t k = 0;
  for (int i = 0; i < ncols; ++i) {
    if (i == next_col_to_remove) {
      delete columns[i];
      do {
        ++k;
        next_col_to_remove = k < n ? cols_to_remove[k] : -1;
      } while (next_col_to_remove == i);
    } else {
      columns[j] = columns[i];
      ++j;
    }
  }
  // This may not be the same as `j` if there were repeating columns
  ncols = j;
  columns.resize(j);
  return this;
}



void DataTable::resize_rows(int64_t new_nrows) {
  if (rowindex) {
    if (new_nrows < nrows) {
      rowindex.shrink(new_nrows, ncols);
      replace_rowindex(rowindex);
      return;
    }
    if (new_nrows > nrows) {
      reify();
      // fall-through
    }
  }
  if (new_nrows != nrows) {
    for (int64_t i = 0; i < ncols; ++i) {
      columns[i]->resize_and_fill(new_nrows);
    }
    nrows = new_nrows;
  }
}



void DataTable::replace_rowindex(const RowIndex& newri) {
  if (newri.isabsent() && rowindex.isabsent()) return;
  rowindex = newri;
  nrows = rowindex.length();
  for (int64_t i = 0; i < ncols; ++i) {
    columns[i]->replace_rowindex(rowindex);
  }
}


void DataTable::replace_groupby(const Groupby& newgb) {
  int32_t last_offset = newgb.offsets_r()[newgb.ngroups()];
  if (last_offset != nrows) {
    throw ValueError() << "Cannot apply Groupby of " << last_offset << " rows "
      "to a Frame with " << nrows << " rows";
  }
  groupby = newgb;
}


void DataTable::set_nkeys(int64_t nk) {
  if (nk < 0) {
    throw ValueError() << "Number of keys cannot be negative: " << nk;
  }
  if (nk > 1) {
    throw NotImplError() << "More than 1 key column is not supported yet";
  }
  if (nk == 0) {
    nkeys = 0;
    return;
  }

  Groupby gb;
  arr32_t cols(static_cast<size_t>(nk));
  for (int32_t i = 0; i < nk; ++i) {
    cols[static_cast<size_t>(i)] = i;
  }
  RowIndex ri = sortby(cols, &gb);
  xassert(ri.length() == nrows);

  if (gb.ngroups() != static_cast<size_t>(nrows)) {
    throw ValueError() << "Cannot set column as a key: the values are not unique";
  }

  replace_rowindex(ri.uplift(rowindex));
  reify();

  nkeys = nk;
}



/**
 * Convert a DataTable view into an actual DataTable. This is done in-place.
 * The resulting DataTable should have a NULL RowIndex and Stats array.
 * Do nothing if the DataTable is not a view.
 */
void DataTable::reify() {
  if (rowindex.isabsent()) return;
  for (int64_t i = 0; i < ncols; ++i) {
    columns[i]->reify();
  }
  rowindex.clear();
}



size_t DataTable::memory_footprint()
{
  size_t sz = 0;
  sz += sizeof(*this);
  sz += static_cast<size_t>(ncols + 1) * sizeof(Column*);
  if (rowindex.isabsent()) {
    for (int i = 0; i < ncols; ++i) {
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
void DataTable::verify_integrity() const {
  // Check that the number of rows/columns in nonnegative
  if (nrows < 0) {
    throw AssertionError()
        << "Frame has a negative value for `nrows`: " << nrows;
  }
  if (ncols < 0) {
    throw AssertionError()
        << "Frame has a negative value for `ncols`: " << ncols;
  }
  if (nkeys < 0) {
    throw AssertionError()
        << "Frame has a negative number of keys: " << nkeys;
  }
  if (nkeys > ncols) {
    throw AssertionError()
        << "Number of keys " << nkeys << " is greater than the number of "
           "columns in the Frame: " << ncols;
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
  for (int64_t i = 0; i < ncols; ++i) {
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

  if (names.size() != static_cast<size_t>(ncols)) {
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
