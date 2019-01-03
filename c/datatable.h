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
#include "python/_all.h"
#include "rowindex.h"
#include "types.h"
#include "column.h"
#include "ztest.h"

// avoid circular dependency between .h files
class Column;
class Stats;
class DataTable;
class NameProvider;

typedef Column* (Column::*colmakerfn)(void) const;
using colvec = std::vector<Column*>;
using strvec = std::vector<std::string>;
using dtptr  = std::unique_ptr<DataTable>;


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
 *     is 2**31 - 1 (even though `ncols` is declared as `size_t`).
 *
 * nkeys
 *     The number of columns that together constitute the primary key of this
 *     data frame. The key columns are always located at the beginning of the
 *     `column` list. The key values are unique, and the frame is sorted by
 *     these values.
 *
 * rowindex
 *     [DEPRECATED]
 *     If this field is not NULL, then the current datatable is a "view", that
 *     is, all columns should be accessed not directly but via this rowindex.
 *     When this field is set, it must be that `nrows == rowindex->length`.
 *
 * columns
 *     The array of columns within the datatable. This array contains `ncols`
 *     elements, and each column has the same number of rows: `nrows`.
 */
class DataTable {
  public:
    size_t   nrows;
    size_t   ncols;
    RowIndex rowindex;  // DEPRECATED (see #1188)
    Groupby  groupby;
    colvec   columns;

  private:
    size_t   nkeys;
    strvec   names;
    mutable py::otuple py_names;   // memoized tuple of column names
    mutable py::odict  py_inames;  // memoized dict of {column name: index}

  public:
    DataTable();
    DataTable(colvec&& cols);
    DataTable(colvec&& cols, const py::olist&);
    DataTable(colvec&& cols, const strvec&);
    DataTable(colvec&& cols, const DataTable*);
    ~DataTable();

    void delete_columns(std::vector<size_t>&);
    void delete_all();
    void resize_rows(size_t n);
    void replace_rowindex(const RowIndex& newri);
    void apply_rowindex(const RowIndex&);
    void replace_groupby(const Groupby& newgb);
    void reify();
    void rbind(std::vector<DataTable*>, std::vector<std::vector<size_t>>);
    DataTable* cbind(std::vector<DataTable*>);
    DataTable* copy() const;
    size_t memory_footprint() const;

    /**
     * Sort the DataTable by specified columns, and return the corresponding
     * RowIndex. The array `colindices` provides the indices of columns to
     * sort on. If an index is negative, it indicates that the column must be
     * sorted in descending order instead of default ascending.
     *
     * If `make_groups` is true, then in addition to sorting, the grouping
     * information will be computed and stored with the RowIndex.
     */
    RowIndex sortby(const std::vector<size_t>& colindices,
                    Groupby* out_grps) const;

    // Names
    const strvec& get_names() const;
    py::otuple get_pynames() const;
    int64_t colindex(const py::_obj& pyname) const;
    size_t xcolindex(const py::_obj& pyname) const;
    void copy_names_from(const DataTable* other);
    void set_names_to_default();
    void set_names(const py::olist& names_list);
    void set_names(const std::vector<std::string>& names_list);
    void replace_names(py::odict replacements);
    void reorder_names(const std::vector<size_t>& col_indices);

    // Key
    size_t get_nkeys() const;
    void set_key(std::vector<size_t>& col_indices);
    void clear_key();
    void set_nkeys_unsafe(size_t K);

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

    static DataTable* load(DataTable* schema, size_t nrows,
                           const std::string& path, bool recode);

    MemoryRange save_jay();
    void save_jay(const std::string& path, WritableBuffer::Strategy);

  private:
    void _init_pynames() const;
    void _set_names_impl(NameProvider*);
    void _integrity_check_names() const;
    void _integrity_check_pynames() const;

    DataTable* _statdt(colmakerfn f) const;
    void save_jay_impl(WritableBuffer*);

    #ifdef DTTEST
      friend void dttest::cover_names_integrity_checks();
    #endif
};


DataTable* open_jay_from_file(const std::string& path);
DataTable* open_jay_from_bytes(const char* ptr, size_t len);
DataTable* open_jay_from_mbuf(const MemoryRange&);

DataTable* apply_rowindex(const DataTable*, const RowIndex& ri);


//==============================================================================

#endif
