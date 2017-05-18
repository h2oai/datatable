#include "colmapping.h"
#include "datatable.h"
#include "py_utils.h"
#include "rowmapping.h"



/**
 * Main "driver" function for the DataTable. Corresponds to DataTable.__call__
 * in Python.
 */
DataTable* dt_DataTable_call(
    DataTable *self, RowMapping *rowmapping, ColMapping *colmapping)
{
    DataTable *res = NULL;
    int64_t ncols = colmapping->length;
    int64_t nrows = rowmapping->length;

    // Computed on-demand only if we detect that it is needed
    RowMapping *merged_rowindex = NULL;

    Column **columns = (Column**) calloc(sizeof(Column*), (size_t)ncols);
    if (columns == NULL) return NULL;

    for (int64_t i = 0; i < ncols; ++i) {
        int64_t j = colmapping->indices[i];
        Column *colj = self->columns[j];
        if (colj->mtype == MT_VIEW) {
            if (merged_rowindex == NULL) {
                merged_rowindex = rowmapping_merge(self->rowmapping, rowmapping);
            }
            ViewColumn *viewcol = (ViewColumn*) TRY(malloc(sizeof(ViewColumn)));
            viewcol->mtype = MT_VIEW;
            viewcol->srcindex = ((ViewColumn*) colj)->srcindex;
            viewcol->stype = colj->stype;
            columns[i] = (Column*) viewcol;
        }
        else if (self->source == NULL) {
            ViewColumn *viewcol = (ViewColumn*) TRY(malloc(sizeof(ViewColumn)));
            viewcol->mtype = MT_VIEW;
            viewcol->srcindex = (size_t) j;
            viewcol->stype = colj->stype;
            columns[i] = (Column*) viewcol;
        }
        else {
            columns[i] = (Column*) TRY(column_extract(colj, rowmapping));
        }
    }

    res = (DataTable*) malloc(sizeof(DataTable));
    if (res == NULL) return NULL;

    res->nrows = nrows;
    res->ncols = ncols;
    res->source = self->source != NULL? self->source : self;
    res->rowmapping = merged_rowindex != NULL? merged_rowindex : rowmapping;
    res->columns = columns;
    return res;

  fail:
    free(columns);
    rowmapping_dealloc(merged_rowindex);
    return NULL;
}



/**
 * Create new DataTable given its number of rows, and the array of `Column`
 * objects.
 */
DataTable* datatable_assemble(int64_t nrows, Column **cols)
{
    if (cols == NULL) return NULL;
    int64_t ncols = 0;
    while(cols[ncols] != NULL) ncols++;

    DataTable *res = (DataTable*) malloc(sizeof(DataTable));
    if (res == NULL) return NULL;
    res->nrows = nrows;
    res->ncols = ncols;
    res->source = NULL;
    res->rowmapping = NULL;
    res->columns = cols;
    return res;
}



DataTable*
datatable_assemble_view(DataTable *src, RowMapping *rm, Column **cols)
{
    if (src == NULL || rm == NULL || cols == NULL) return NULL;
    int64_t ncols = 0;
    while(cols[ncols] != NULL) ncols++;

    DataTable *res = (DataTable*) malloc(sizeof(DataTable));
    if (res == NULL) return NULL;
    res->nrows = rm->length;
    res->ncols = ncols;
    res->source = src;
    res->rowmapping = rm;
    res->columns = cols;
    return res;
}



/**
 * Free memory occupied by the :class:`DataTable` object. This function should
 * be called from `DataTable_PyObject`s deallocator only.
 */
void datatable_dealloc(DataTable *self)
{
    if (self == NULL) return;

    self->source = NULL;  // .source reference is not owned by this object
    rowmapping_dealloc(self->rowmapping);
    for (int64_t i = 0; i < self->ncols; i++) {
        column_dealloc(self->columns[i]);
    }
    free(self->columns);
    free(self);
}
