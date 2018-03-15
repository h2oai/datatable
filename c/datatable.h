//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_DATATABLE_h
#define dt_DATATABLE_h
#include <vector>
#include <string>
#include "datatable_check.h"
#include "rowindex.h"
#include "types.h"
#include "column.h"

// avoid circular dependency between .h files
class Column;
class Stats;
class DataTable;

typedef std::unique_ptr<DataTable> DataTablePtr;
typedef Column* (Column::*colmakerfn)(void) const;


//==============================================================================

/**
 * The DataTable
 *
 * Properties
 * ----------
 * nrows
 * ncols
 *     Data dimensions: number of rows and columns in the datatable. We do not
 *     support more than 2 dimensions (as Numpy or TensorFlow do).
 *     The maximum number of rows is 2**63 - 1. The maximum number of columns
 *     is 2**31 - 1 (even though `ncols` is declared as `int64_t`).
 *
 * rowindex
 *     If this field is not NULL, then the current datatable is a "view", that
 *     is, all columns should be accessed not directly but via this rowindex.
 *     When this field is set, it must be that `nrows == rowindex->length`.
 *
 * columns
 *     The array of columns within the datatable. This array contains `ncols+1`
 *     elements, and each column has the same number of rows: `nrows`. The
 *     "extra" column is always NULL: a sentinel.
 *     When `rowindex` is specified, then each column is a copy-by-reference
 *     of a column in some other datatable, and only indices given in the
 *     `rowindex` should be used to access values in each column.
 */
class DataTable {
  public:
    int64_t  nrows;
    int64_t  ncols;
    RowIndex rowindex;
    Column** columns;

  public:
    DataTable(Column**);
    ~DataTable();
    DataTable* delete_columns(int*, int);
    void apply_na_mask(DataTable* mask);
    void replace_rowindex(const RowIndex& newri);
    void reify();
    DataTable* rbind(DataTable**, int**, int, int64_t);
    DataTable* cbind(DataTable**, int);
    size_t memory_footprint();

    /**
     * Sort the DataTable by specified columns, and return the corresponding
     * RowIndex. The array `colindices` provides the indices of columns to
     * sort on. If an index is negative, it indicates that the column must be
     * sorted in descending order instead of default ascending.
     *
     * If `make_groups` is true, then in addition to sorting, the grouping
     * information will be computed and stored with the RowIndex.
     */
    RowIndex sortby(const arr32_t& colindices, bool make_groups) const;

    DataTable* min_datatable() const;
    DataTable* max_datatable() const;
    DataTable* mode_datatable() const;
    DataTable* sum_datatable() const;
    DataTable* mean_datatable() const;
    DataTable* sd_datatable() const;
    DataTable* countna_datatable() const;
    DataTable* nunique_datatable() const;
    DataTable* nmodal_datatable() const;

    bool verify_integrity(IntegrityCheckContext& icc) const;

    static DataTable* load(DataTable* schema, int64_t nrows,
                           const std::string& path);

  private:
    DataTable* _statdt(colmakerfn f) const;
};



//==============================================================================

#endif
