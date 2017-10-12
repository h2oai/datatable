#include <stdlib.h>  // abs
#include <string.h>  // memcpy
#include "datatable.h"
#include "myassert.h"
#include "utils.h"



/**
 * Load DataTable stored in NFF format on disk.
 *
 * colspec
 *     A DataTable object containing information about the columns of the
 *     datatable stored on disk. This object should contain 3 columns, with
 *     file names, stypes, and meta-information of each column in the stored
 *     datatable.
 *
 * nrows
 *     Number of rows in the stored datatable.
 */
DataTable* DataTable::load(DataTable *colspec, int64_t nrows)
{
    int64_t ncols = colspec->nrows;
    Column **columns = NULL;
    dtmalloc(columns, Column*, ncols + 1);
    columns[ncols] = NULL;

    if (colspec->ncols != 3) {
        throw new Error("colspec table should have had 3 columns, but %lld passed",
               colspec->ncols);
    }
    Column *colf = colspec->columns[0];
    Column *cols = colspec->columns[1];
    Column *colm = colspec->columns[2];
    if (colf->stype() != ST_STRING_I4_VCHAR ||
        cols->stype() != ST_STRING_I4_VCHAR ||
        colm->stype() != ST_STRING_I4_VCHAR) {
        throw new Error("String columns are expected in colspec table, instead got "
               "%d, %d and %d", colf->stype(), cols->stype(), colm->stype());
    }

    int64_t oof = ((VarcharMeta*) colf->meta)->offoff;
    int64_t oos = ((VarcharMeta*) cols->meta)->offoff;
    int64_t oom = ((VarcharMeta*) colm->meta)->offoff;
    int32_t *offf = (int32_t*) colf->data_at(static_cast<size_t>(oof));
    int32_t *offs = (int32_t*) cols->data_at(static_cast<size_t>(oos));
    int32_t *offm = (int32_t*) colm->data_at(static_cast<size_t>(oom));

    static char filename[101];
    static char metastr[101];
    for (int64_t i = 0; i < ncols; ++i)
    {
        // Extract filename
        int32_t fsta = abs(offf[i - 1]) - 1;
        int32_t fend = abs(offf[i]) - 1;
        int32_t flen = fend - fsta;
        if (flen > 100) throw new Error("Filename is too long: %d", flen);
        memcpy(filename, colf->data_at(static_cast<size_t>(fsta)), (size_t) flen);
        filename[flen] = '\0';

        // Extract stype
        int32_t ssta = abs(offs[i - 1]) - 1;
        int32_t send = abs(offs[i]) - 1;
        int32_t slen = send - ssta;
        if (slen != 3) throw new Error("Incorrect stype's length: %d", slen);
        SType stype = stype_from_string((char*)cols->data() + (ssize_t)ssta);
        if (stype == ST_VOID) {
            char stypestr[4];
            memcpy(stypestr, cols->data_at(static_cast<size_t>(ssta)), 3);
            stypestr[3] = '\0';
            throw new Error("Unrecognized stype: %s", stypestr);
        }

        // Extract meta info (as a string)
        int32_t msta = abs(offm[i - 1]) - 1;
        int32_t mend = abs(offm[i]) - 1;
        int32_t mlen = mend - msta;
        if (mlen > 100) throw new Error("Meta string is too long: %d", mlen);
        memcpy(metastr, colm->data_at(static_cast<size_t>(msta)), (size_t) mlen);
        metastr[mlen] = '\0';

        // Load the column
        columns[i] = Column::open_mmap_column(stype, nrows, filename, metastr);
    }

    return new DataTable(columns);
}
