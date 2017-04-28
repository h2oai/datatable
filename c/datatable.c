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
    int64_t ncols = colmapping->length;
    int64_t nrows = rowmapping->length;

    // Computed on-demand only if we detect that it is needed
    RowMapping *merged_rowindex = NULL;

    Column **columns = calloc(sizeof(Column*), (size_t)ncols);
    if (columns == NULL) return NULL;

    for (int64_t i = 0; i < ncols; ++i) {
        int64_t j = colmapping->indices[i];
        Column *colj = self->columns[j];
        if (colj->mtype == MT_VIEW) {
            if (merged_rowindex == NULL) {
                merged_rowindex = rowmapping_merge(self->rowmapping, rowmapping);
            }
            ViewColumn *viewcol = TRY(malloc(sizeof(ViewColumn)));
            viewcol->mtype = MT_VIEW;
            viewcol->srcindex = ((ViewColumn*) colj)->srcindex;
            viewcol->stype = colj->stype;
            columns[i] = (Column*) viewcol;
        }
        else if (self->source == NULL) {
            ViewColumn *viewcol = TRY(malloc(sizeof(ViewColumn)));
            viewcol->mtype = MT_VIEW;
            viewcol->srcindex = (size_t) j;
            viewcol->stype = colj->stype;
            columns[i] = (Column*) viewcol;
        }
        else {
            columns[i] = TRY(column_extract(colj, rowmapping));
        }
    }

    DataTable *res = malloc(sizeof(DataTable));
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
