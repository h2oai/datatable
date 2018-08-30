//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "datatable.h"
#include <stdlib.h>
#include "utils/omp.h"
#include "py_utils.h"
#include "rowindex.h"
#include "types.h"

// Forward declarations
static int _compare_ints(const void *a, const void *b);



DataTable::DataTable(Column** cols)
  : nrows(0),
    ncols(0),
    nkeys(0),
    columns(cols)
{
  if (cols == nullptr) {
    throw ValueError() << "Column array cannot be null";
  }
  if (cols[0] == nullptr) return;
  rowindex = RowIndex(cols[0]->rowindex());
  nrows = cols[0]->nrows;

  bool need_to_materialize = false;
  for (ncols = 1; ; ++ncols) {
    Column* col = cols[ncols];
    if (!col) break;
    if (rowindex != col->rowindex()) {
      need_to_materialize = true;
    }
    if (nrows != col->nrows) {
      throw ValueError() << "Mismatched length in Column " << ncols << ": "
                         << "found " << col->nrows << ", expected " << nrows;
    }
  }
  // TODO: remove in #1188
  if (need_to_materialize) {
    reify();
  }
}



DataTable* DataTable::delete_columns(int *cols_to_remove, int64_t n)
{
  if (n == 0) return this;
  qsort(cols_to_remove, static_cast<size_t>(n), sizeof(int), _compare_ints);
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
  columns[j] = nullptr;
  // This may not be the same as `j` if there were repeating columns
  ncols = j;
  columns = dt::realloc(columns, sizeof(Column*) * static_cast<size_t>(j + 1));
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
 * Free memory occupied by the :class:`DataTable` object. This function should
 * be called from `pydatatable::obj`s deallocator only.
 */
DataTable::~DataTable()
{
  for (int64_t i = 0; i < ncols; ++i) {
    delete columns[i];
  }
  delete columns;
}


// Comparator function to sort integers using `qsort`
static inline int _compare_ints(const void *a, const void *b) {
  const int x = *static_cast<const int*>(a);
  const int y = *static_cast<const int*>(b);
  return (x > y) - (x < y);
}


/**
 * Modify datatable replacing values that are given by the mask with NAs.
 * The target datatable must have the same shape as the mask, and neither can
 * be a view.
 */
void DataTable::apply_na_mask(DataTable* maskdt)
{
  if (!maskdt) {
    throw ValueError() << "Mask cannot be NULL";
  }
  if (ncols != maskdt->ncols || nrows != maskdt->nrows) {
    throw ValueError() << "Target datatable and mask have different shapes";
  }
  if (!(rowindex.isabsent() || maskdt->rowindex.isabsent())) {
    throw ValueError() << "Neither target DataTable nor the mask can be views";
  }

  for (int64_t i = 0; i < ncols; ++i){
    BoolColumn *maskcol = dynamic_cast<BoolColumn*>(maskdt->columns[i]);
    if (!maskcol) {
      throw ValueError() << "Column " << i
                         << " in mask is not of a boolean type";
    }
    Column *col = columns[i];
    col->apply_na_mask(maskcol);
  }
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
  Column** out_cols = new Column*[ncols + 1];
  for (int64_t i = 0; i < ncols; ++i) {
    out_cols[i] = (columns[i]->*f)();
  }
  out_cols[ncols] = nullptr;
  DataTable* res = new DataTable(out_cols);
  res->names = names;
  return res;
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

  // Check the number of columns; the number of allocated columns should be
  // equal to `ncols + 1` (with extra column being NULL). Sometimes the
  // allocation size can be greater than the required number of columns,
  // because `malloc()` may allocate more than requested.
  size_t n_cols_allocd = array_size(columns, sizeof(Column*));
  if (!columns || !n_cols_allocd) {
    throw AssertionError() << "DataTable.columns array of is not allocated";
  }
  else if (ncols + 1 > static_cast<int64_t>(n_cols_allocd)) {
    throw AssertionError()
        << "DataTable.columns array size is " << n_cols_allocd
        << " whereas " << ncols + 1 << " columsn are expected.";
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

  if (columns[ncols] != nullptr) {
    // Memory was allocated for `ncols+1` columns, but the last element
    // was not set to NULL.
    // Note that if `cols` array was under-allocated and `malloc_size`
    // not available on this platform, then this might segfault... This is
    // unavoidable since if we skip the check and do `cols[ncols]` later on
    // then we will segfault anyways.
    throw AssertionError()
        << "Last entry in the `columns` array of Frame is not null";
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
