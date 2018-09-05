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
#include "python/dict.h"
#include "python/tuple.h"
#include "rowindex.h"
#include "types.h"
#include "column.h"
#include "ztest.h"

// avoid circular dependency between .h files
class Column;
class Stats;
class DataTable;
class NameProvider;

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
    int64_t  nkeys;
    RowIndex rowindex;
    Groupby  groupby;
    Column** columns;

  private:
    std::vector<std::string> names;
    mutable py::otuple py_names;   // memoized tuple of column names
    mutable py::odict  py_inames;  // memoized dict of {column name: index}

  public:
    DataTable(Column**);
    ~DataTable();
    DataTable* delete_columns(int*, int64_t);
    void resize_rows(int64_t n);
    void apply_na_mask(DataTable* mask);
    void replace_rowindex(const RowIndex& newri);
    void replace_groupby(const Groupby& newgb);
    void reify();
    void rbind(DataTable**, int**, int64_t, int64_t);
    DataTable* cbind(DataTable**, int64_t);
    DataTable* copy() const;
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
    RowIndex sortby(const arr32_t& colindices, Groupby* out_grps) const;

    const std::vector<std::string>& get_names() const;
    py::otuple get_pynames() const;
    int64_t colindex(const py::_obj& pyname) const;
    void copy_names_from(const DataTable* other);
    void set_names_to_default();
    void set_names(py::olist names_list);
    void set_names(const std::vector<std::string>& names_list);
    void replace_names(py::odict replacements);

    void set_nkeys(int64_t nk);

    DataTable* min_datatable() const;
    DataTable* max_datatable() const;
    DataTable* mode_datatable() const;
    DataTable* sum_datatable() const;
    DataTable* mean_datatable() const;
    DataTable* sd_datatable() const;
    DataTable* skew_datatable() const;
    DataTable* kurt_datatable() const;
    DataTable* countna_datatable() const;
    DataTable* nunique_datatable() const;
    DataTable* nmodal_datatable() const;

    void verify_integrity() const;

    static DataTable* load(DataTable* schema, int64_t nrows,
                           const std::string& path, bool recode);

    void save_jay(const std::string& path,
                  const std::vector<std::string>& colnames,
                  WritableBuffer::Strategy wstrategy);
    static DataTable* open_jay(const std::string& path);

  private:
    void _init_pynames() const;
    void _set_names_impl(NameProvider*);
    void _integrity_check_names() const;
    void _integrity_check_pynames() const;

    DataTable* _statdt(colmakerfn f) const;

    friend void dttest::cover_names_integrity_checks();
};



//==============================================================================

#endif
