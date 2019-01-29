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

struct sort_spec {
  size_t col_index;
  bool descending;
  bool na_last;
  bool sort_only;
  size_t : 40;

  sort_spec(size_t i)
    : col_index(i), descending(false), na_last(false), sort_only(false) {}
  sort_spec(size_t i, bool desc, bool nalast, bool sort)
    : col_index(i), descending(desc), na_last(nalast), sort_only(sort) {}
};

struct RowColIndex {
  RowIndex rowindex;
  std::vector<size_t> colindices;
};

typedef Column* (Column::*colmakerfn)(void) const;
using colvec = std::vector<Column*>;
using intvec = std::vector<size_t>;
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
    Groupby  groupby;
    colvec   columns;

  private:
    RowIndex rowindex;  // DEPRECATED (see #1188)
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

    void delete_columns(intvec&);
    void delete_all();
    void resize_rows(size_t n);
    void replace_rowindex(const RowIndex& newri);
    void apply_rowindex(const RowIndex&);
    void replace_groupby(const Groupby& newgb);
    void reify();
    void rbind(std::vector<DataTable*>, std::vector<intvec>);
    DataTable* cbind(std::vector<DataTable*>);
    DataTable* copy() const;
    size_t memory_footprint() const;

    /**
     * Sort the DataTable by specified columns, and return the corresponding
     * RowIndex. The array `colindices` provides the indices of columns to
     * sort on.
     *
     * If `make_groups` is true, then in addition to sorting, the grouping
     * information will be computed and stored with the RowIndex.
     */
    // TODO: remove
    RowIndex sortby(const intvec& colindices, Groupby* out_grps) const;

    std::pair<RowIndex, Groupby>
    group(const std::vector<sort_spec>& spec, bool as_view = false) const;

    // Names
    const strvec& get_names() const;
    py::otuple get_pynames() const;
    int64_t colindex(const py::_obj& pyname) const;
    size_t xcolindex(const py::_obj& pyname) const;
    void copy_names_from(const DataTable* other);
    void set_names_to_default();
    void set_names(const py::olist& names_list);
    void set_names(const strvec& names_list);
    void replace_names(py::odict replacements);
    void reorder_names(const intvec& col_indices);

    // Key
    size_t get_nkeys() const;
    void set_key(intvec& col_indices);
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

    std::vector<RowColIndex> split_columns_by_rowindices() const;

    /**
     * `iterate_rows<F1, FN>(row0, row1)` is a helper function to iterate over
     * the rows of a DataTable. It takes two functors as parameters:
     *
     *   - `void F1(size_t i, size_t j)` is used for iterating over a DataTable
     *     that either has no rowindex, or a uniform (across all rows) rowindex.
     *     This function takes two parameters: `i`, going from 0 to `nrows - 1`,
     *     is the index of the current row; and `j` is the index within each
     *     column where the data for row `i` is located.
     *
     *   - `void FN(size_t i, const intvec& js)` is used for iterating over a
     *     more generic DataTable. Here `i` is again the index of the current
     *     row, and the vector `js` contains indices for each column where the
     *     value for that column has to be located.
     */
    template <void (*F1)(size_t i, size_t j),
              void (*FN)(size_t i, const intvec& js)>
    void iterate_rows(size_t row0 = 0, size_t row1 = size_t(-1));

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

RowIndex natural_join(const DataTable* xdt, const DataTable* jdt);


//==============================================================================

template <void (*F1)(size_t i, size_t j),
          void (*FN)(size_t i, const intvec& js)>
void DataTable::iterate_rows(size_t row0, size_t row1) {
  if (row1 == size_t(-1)) {
    row1 = nrows;
  }
  std::vector<RowColIndex> rcs = split_columns_by_rowindices();
  if (rcs.size() == 1) {
    RowIndex ri0 = rcs[0].rowindex;
    ri0.iterate(row0, row1, 1, F1);
  }
  else {
    std::vector<size_t> js(ncols);
    for (size_t i = row0; i < row1; ++i) {
      for (const auto& rcitem : rcs) {
        size_t j = rcitem.rowindex[i];
        for (size_t k : rcitem.colindices) {
          js[k] = j;
        }
      }
      FN(i, js);
    }
  }
}


#endif
