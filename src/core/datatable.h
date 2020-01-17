//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#ifndef dt_DATATABLE_h
#define dt_DATATABLE_h
#include <memory>         // std::unique_ptr
#include <string>         // std::string
#include <vector>         // std::vector
#include "python/_all.h"
#include "groupby.h"
#include "rowindex.h"
#include "types.h"
#include "column.h"
#include "ztest.h"

// avoid circular dependency between .h files
class Column;
class Stats;
class DataTable;
class NameProvider;


using colvec = std::vector<Column>;
using intvec = std::vector<size_t>;
using strvec = std::vector<std::string>;


//==============================================================================

/**
  * The DataTable
  *
  * Properties
  * ----------
  * nrows_
  * ncols_
  *     Data dimensions: number of rows and columns in the datatable. We do not
  *     support more than 2 dimensions (as Numpy or TensorFlow do).
  *     The maximum number of rows is 2**63 - 1. The maximum number of columns
  *     is 2**31 - 1 (even though `ncols_` is declared as `size_t`).
  *
  * nkeys_
  *     The number of columns that together constitute the primary key of this
  *     data frame. The key columns are always located at the beginning of the
  *     `columns_` list. The key values are unique, and the frame is sorted by
  *     these values.
  *
  * columns_
  *     The array of columns within the datatable. This array contains `ncols_`
  *     elements, and each column has the same number of rows: `nrows_`.
  */
class DataTable {
  private:
    size_t  nrows_;
    size_t  ncols_;
    size_t  nkeys_;
    colvec  columns_;
    strvec  names_;
    mutable py::otuple py_names_;   // memoized tuple of column names
    mutable py::odict  py_inames_;  // memoized dict of {column name: index}

  public:
    static struct DefaultNamesTag {} default_names;
    static struct DeepCopyTag {} deep_copy;

    DataTable();
    DataTable(const DataTable&);
    DataTable(DataTable&&) = default;
    DataTable(const DataTable&, DeepCopyTag);
    DataTable(colvec&& cols, DefaultNamesTag);
    DataTable(colvec&& cols, const strvec&, bool warn_duplicates = true);
    DataTable(colvec&& cols, const py::olist&, bool warn_duplicates = true);
    DataTable(colvec&& cols, const DataTable&);

    size_t nrows() const noexcept { return nrows_; }
    size_t ncols() const noexcept { return ncols_; }
    size_t nkeys() const noexcept { return nkeys_; }

    void delete_columns(intvec&);
    void delete_all();
    void resize_rows(size_t n);
    void resize_columns(const strvec& new_names);
    void apply_rowindex(const RowIndex&);
    void materialize(bool to_memory);
    void rbind(const std::vector<DataTable*>&, const std::vector<intvec>&);
    void cbind(const std::vector<DataTable*>&);
    DataTable extract_column(size_t i) const;
    size_t memory_footprint() const noexcept;

    const Column& get_column(size_t i) const;
    Column& get_column(size_t i);
    void set_column(size_t i, Column&& newcol);

    // Names
    const strvec& get_names() const;
    py::otuple get_pynames() const;
    int64_t colindex(const py::_obj& pyname) const;
    size_t xcolindex(const py::_obj& pyname) const;
    size_t xcolindex(int64_t index) const;
    void copy_names_from(const DataTable& other);
    void set_names_to_default();
    void set_names(const py::olist& names_list, bool warn = true);
    void set_names(const strvec& names_list, bool warn = true);
    void replace_names(py::odict replacements, bool warn = true);
    void reorder_names(const intvec& col_indices);

    // Key
    void set_key(intvec& col_indices);
    void clear_key();
    void set_nkeys_unsafe(size_t K);

    void verify_integrity() const;

    Buffer save_jay();
    void save_jay(const std::string& path, WritableBuffer::Strategy);

  private:
    DataTable(colvec&& cols);

    void _init_pynames() const;
    void _set_names_impl(NameProvider*, bool warn);
    void _integrity_check_names() const;
    void _integrity_check_pynames() const;

    void save_jay_impl(WritableBuffer*);

    #ifdef DTTEST
      friend void dttest::cover_names_integrity_checks();
    #endif
};



DataTable* open_jay_from_file(const std::string& path);
DataTable* open_jay_from_bytes(const char* ptr, size_t len);
DataTable* open_jay_from_mbuf(const Buffer&);

RowIndex natural_join(const DataTable& xdt, const DataTable& jdt);


#endif
