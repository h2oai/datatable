#include <stdlib.h>
#include "myomp.h"
#include "datatable.h"
#include "py_utils.h"
#include "types.h"
#include "rowindex.h"

// Forward declarations
static int _compare_ints(const void *a, const void *b);


/**
 * Create new DataTable given the set of columns, and a rowindex. The `rowindex`
 * object may also be NULL, in which case a DataTable without a rowindex will
 * be constructed.
 */
DataTable::DataTable(Column **cols) :
    nrows(0),
    ncols(0),
    rowindex(nullptr),
    columns(cols)
{
    if (cols == nullptr)
        throw new Error("Column array cannot be null");
    if (cols[ncols] == nullptr) return;
    rowindex = cols[ncols]->rowindex();
    nrows = cols[ncols]->nrows;

    for (Column* col = cols[++ncols]; cols[ncols] != nullptr; ++ncols) {
        if (rowindex != col->rowindex())
            throw new Error("Mismatched RowIndex in Column %" PRId64, ncols);
        if (nrows != col->nrows)
            throw new Error("Mismatched length in Column %" PRId64 ": "
                            "found %" PRId64 ", expected %" PRId64,
                            ncols, col->nrows, nrows);
    }
}



DataTable* DataTable::delete_columns(int *cols_to_remove, int n)
{
    if (n == 0) return this;
    qsort(cols_to_remove, (size_t)n, sizeof(int), _compare_ints);
    int j = 0;
    int next_col_to_remove = cols_to_remove[0];
    int k = 0;
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
    columns[j] = NULL;
    // This may not be the same as `j` if there were repeating columns
    ncols = j;
    columns = static_cast<Column**>(realloc(columns, sizeof(Column*) * (size_t) (j + 1)));
    return this;
}



/**
 * Free memory occupied by the :class:`DataTable` object. This function should
 * be called from `DataTable_PyObject`s deallocator only.
 */
DataTable::~DataTable()
{
    if (rowindex) rowindex->release();
    for (int64_t i = 0; i < ncols; ++i) {
        delete columns[i];
    }
    delete columns;
}


// Comparator function to sort integers using `qsort`
static inline int _compare_ints(const void *a, const void *b) {
    const int x = *(const int*)a;
    const int y = *(const int*)b;
    return (x > y) - (x < y);
}


/**
 * Modify datatable replacing values that are given by the mask with NAs.
 * The target datatable must have the same shape as the mask, and neither can
 * be a view.
 * Returns NULL in case of an error, or a pointer to `dt` otherwise.
 */
DataTable* DataTable::apply_na_mask(DataTable *mask)
{
    if(mask == NULL) throw new Error("Mask cannot be NULL");
    if (ncols != mask->ncols || nrows != mask->nrows) {
        throw new Error("Target datatable and mask have different shapes");
    }
    if (rowindex || mask->rowindex) {
        throw new Error("Neither target DataTable nor a mask can be views");
    }
    for (int64_t i = 0; i < ncols; ++i) {
        if (mask->columns[i]->stype() != ST_BOOLEAN_I1)
            dterrv("Column %lld in mask is not of a boolean type", i);
    }

    for (int64_t i = 0; i < ncols; ++i){
        // TODO: Move this part into columns.cc?
        Column *col = columns[i];
        col->stats->reset();
        uint8_t *mdata = (uint8_t*) mask->columns[i]->data();
        switch (col->stype()) {
            case ST_BOOLEAN_I1:
            case ST_INTEGER_I1: {
                uint8_t *cdata = (uint8_t*) col->data();
                #pragma omp parallel for schedule(dynamic,1024)
                for (int64_t j = 0; j < nrows; ++j) {
                    if (mdata[j])
                        cdata[j] = static_cast<uint8_t>(GETNA<int8_t>());
                }
                break;
            }
            case ST_INTEGER_I2: {
                uint16_t *cdata = (uint16_t*) col->data();
                #pragma omp parallel for schedule(dynamic,1024)
                for (int64_t j = 0; j < nrows; ++j) {
                    if (mdata[j])
                        cdata[j] = static_cast<uint16_t>(GETNA<int16_t>());
                }
                break;
            }
            case ST_REAL_F4:
            case ST_INTEGER_I4: {
                uint32_t *cdata = (uint32_t*) col->data();
                uint32_t na = col->stype() == ST_REAL_F4 ?
                              NA_F4_BITS :
                              static_cast<uint32_t>(GETNA<int32_t>());
                #pragma omp parallel for schedule(dynamic,1024)
                for (int64_t j = 0; j < nrows; ++j) {
                    if (mdata[j]) cdata[j] = na;
                }
                break;
            }
            case ST_REAL_F8:
            case ST_INTEGER_I8: {
                uint64_t *cdata = (uint64_t*) col->data();
                uint64_t na = col->stype() == ST_REAL_F8 ?
                              NA_F8_BITS :
                              static_cast<uint32_t>(GETNA<int32_t>());
                #pragma omp parallel for schedule(dynamic,1024)
                for (int64_t j = 0; j < nrows; ++j) {
                    if (mdata[j]) cdata[j] = na;
                }
                break;
            }
            case ST_STRING_I4_VCHAR: {
                int64_t offoff = ((VarcharMeta*) col->meta)->offoff;
                char *strdata = (char*)(col->data()) - 1;
                int32_t *offdata = static_cast<int32_t*>(col->data_at(static_cast<size_t>(offoff)));
                // How much to reduce the offsets by due to some strings being
                // converted into NAs
                int32_t doffset = 0;
                for (int64_t j = 0; j < nrows; ++j) {
                    int32_t offi = offdata[j];
                    int32_t offp = abs(offdata[j - 1]);
                    if (mdata[j]) {
                        doffset += abs(offi) - offp;
                        offdata[j] = -offp;
                    } else if (doffset) {
                        if (offi > 0) {
                            offdata[j] = offi - doffset;
                            memmove(strdata + offp, strdata + offp + doffset,
                                    static_cast<size_t>((offi - offp - doffset)));
                        } else {
                            offdata[j] = -offp;
                        }
                    }
                }
                break;
            }
            default:
                throw new Error("Column type %d not supported", col->stype());
        }

    }

    return this;
}

/**
 * Convert a DataTable view into an actual DataTable. This is done in-place.
 * The resulting DataTable should have a NULL RowIndex and Stats array.
 * Do nothing if the DataTable is not a view.
 */
void DataTable::reify() {
    if (rowindex == nullptr) return;
    for (int64_t i = 0; i < ncols; ++i) {
        Column *newcol = columns[i]->extract();
        delete columns[i];
        columns[i] = newcol;
    }
    rowindex->release();
    rowindex = nullptr;
}



size_t DataTable::memory_footprint()
{
    size_t sz = 0;
    sz += sizeof(*this);
    sz += (size_t)(ncols + 1) * sizeof(Column*);
    if (rowindex) {
        // If table is a view, then ignore sizes of each individual column.
        sz += rowindex->alloc_size();
    } else {
        for (int i = 0; i < ncols; ++i) {
            sz += columns[i]->memory_footprint();
        }
    }
    return sz;
}
