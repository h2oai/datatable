//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
#ifndef dt_DATATABLE_H
#define dt_DATATABLE_H
#include <inttypes.h>
#include <vector>
#include <string>
#include "datatable_check.h"
#include "types.h"
#include "column.h"

// avoid circular dependency between .h files
class RowIndex;
typedef struct ColMapping ColMapping;
class Column;
class Stats;
class DataTable;

typedef std::unique_ptr<DataTable> DataTablePtr;


//==============================================================================

/**
 * The DataTable
 *
 * Parameters
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
    int64_t     nrows;
    int64_t     ncols;
    RowIndex   *rowindex;
    Column    **columns;

    DataTable(Column**);
    ~DataTable();
    DataTable* delete_columns(int*, int);
    void apply_na_mask(DataTable* mask);
    void reify();
    DataTable* rbind(DataTable**, int**, int, int64_t);
    DataTable* cbind(DataTable**, int);
    size_t memory_footprint();

    DataTable* min_datatable() const;
    DataTable* max_datatable() const;
    DataTable* sum_datatable() const;
    DataTable* mean_datatable() const;
    DataTable* sd_datatable() const;
    DataTable* countna_datatable() const;

    bool verify_integrity(IntegrityCheckContext& icc) const;

    static DataTable* load(DataTable* schema, int64_t nrows, const std::string& path);
};



//==============================================================================

#endif
