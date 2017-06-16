#include <assert.h>
#include <string.h>  // memcpy
#include "fread.h"
#include "utils.h"
#include "py_datatable.h"
#include "py_utils.h"


static const size_t colTypeSizes[NUMTYPE] = {0, 1, 4, 4, 8, 8, 8};
static const SType colType_to_stype[NUMTYPE] = {
    ST_VOID,
    ST_BOOLEAN_I1,
    ST_INTEGER_I4,
    ST_INTEGER_I4,
    ST_INTEGER_I8,
    ST_REAL_F8,
    ST_STRING_I4_VCHAR,
};

typedef struct StrBuf {
    char *buf;
    size_t ptr;
    size_t size;
} StrBuf;

// Forward declarations
void cleanup_fread_session(freadMainArgs *frargs);



// Python FReader object, which holds specifications for the current reader
// logic. This reference is non-NULL when fread() is running; and serves as a
// lock preventing from running multiple fread() instances.
static PyObject *freader = NULL;

// DataTable being constructed.
static DataTable *dt = NULL;

// List of column names for the final DataTable
static PyObject *colNamesList = NULL;

static char *filename = NULL;
static char *input = NULL;
static char **na_strings = NULL;

static int64_t ncols = 0;
static int8_t *types = NULL;
static int8_t *sizes = NULL;
static StrBuf **strbufs = NULL;


/**
 * Python wrapper around `freadMain()`. This function extracts the arguments
 * from the provided :class:`FReader` python object, converts them into the
 * `freadMainArgs` structure, and then passes that structure to the `freadMain`
 * function.
 */
PyObject* freadPy(UU, PyObject *args)
{
    if (freader != NULL) {
        PyErr_SetString(PyExc_RuntimeError,
            "Cannot run multiple instances of fread() in-parallel.");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "O:fread", &freader))
        goto fail;
    Py_INCREF(freader);

    freadMainArgs frargs;

    filename = TOSTRING(ATTR(freader, "filename"), NULL);
    input = TOSTRING(ATTR(freader, "text"), NULL);
    na_strings = TOSTRINGLIST(ATTR(freader, "na_strings"), NULL);
    frargs.filename = filename;
    frargs.input = input;
    frargs.sep = TOCHAR(ATTR(freader, "sep"), 0);
    frargs.dec = '.';
    frargs.quote = '"';
    frargs.nrowLimit = TOINT64(ATTR(freader, "max_nrows"), 0);
    frargs.skipNrow = 0;
    frargs.skipString = NULL;
    frargs.header = TOBOOL(ATTR(freader, "header"), NA_BOOL8);
    frargs.verbose = TOBOOL(ATTR(freader, "verbose"), 0);
    frargs.NAstrings = (const char* const*) na_strings;
    frargs.stripWhite = 1;
    frargs.skipEmptyLines = 1;
    frargs.fill = TOBOOL(ATTR(freader, "fill"), 0);
    frargs.showProgress = 0;
    frargs.nth = -1;
    frargs.warningsAreErrors = 0;
    if (frargs.nrowLimit < 0)
        frargs.nrowLimit = LONG_MAX;

    frargs.freader = freader;
    Py_INCREF(freader);

    int res = freadMain(frargs);
    if (!res) goto fail;

    PyObject *pydt = pydt_from_dt(dt);
    if (pydt == NULL) goto fail;
    cleanup_fread_session(&frargs);
    return pydt;

  fail:
    datatable_dealloc(dt);
    cleanup_fread_session(&frargs);
    return NULL;
}



void cleanup_fread_session(freadMainArgs *frargs) {
    dtfree(filename);
    dtfree(input);
    if (frargs) {
        if (na_strings) {
            char **ptr = na_strings;
            while (*ptr++) dtfree(*ptr);
            dtfree(na_strings);
        }
        pyfree(frargs->freader);
    }
    if (strbufs) {
        for (int64_t i = 0; i < ncols; i++) {
            if (strbufs[i]) {
                dtfree(strbufs[i]->buf);
                dtfree(strbufs[i]);
            }
        }
        dtfree(strbufs);
    }
    pyfree(freader);
    pyfree(colNamesList);
    dt = NULL;
}



_Bool userOverride(
    UNUSED(int8_t *types_), lenOff *colNames, const char *anchor, int ncols_
) {
    colNamesList = PyTuple_New(ncols_);
    for (int i = 0; i < ncols_; i++) {
        lenOff ocol = colNames[i];
        PyObject *col =
            ocol.len > 0? PyUnicode_FromStringAndSize(anchor + ocol.off, ocol.len)
                        : PyUnicode_FromFormat("V%d", i);
        PyTuple_SET_ITEM(colNamesList, i, col);
    }
    PyObject_SetAttrString(freader, "_colnames", colNamesList);

    return 1;  // continue reading the file
}


/**
 * Allocate memory for the datatable that will be read.
 */
size_t allocateDT(int8_t *types_, int8_t *sizes_, int ncols_, int ndrop,
                  size_t nrows)
{
    Column **columns = NULL;
    types = types_;
    sizes = sizes_;
    ncols = (int64_t) ncols_;

    dtcalloc_g(columns, Column*, ncols - ndrop + 1);
    dtcalloc_g(strbufs, StrBuf*, ncols);
    columns[ncols - ndrop] = NULL;

    size_t total_alloc_size = sizeof(Column*) * (size_t)(ncols - ndrop) +
                              sizeof(StrBuf*) * (size_t)(ncols - ndrop);
    for (int i = 0, j = 0; i < ncols_; i++) {
        int8_t type = types[i];
        if (type == CT_DROP)
            continue;
        size_t alloc_size = colTypeSizes[type] * nrows;
        dtmalloc_g(columns[j], Column, 1);
        dtmalloc_g(columns[j]->data, void, alloc_size);
        columns[j]->mtype = MT_DATA;
        columns[j]->stype = ST_VOID; // stype will be 0 until the column is finalized
        columns[j]->meta = NULL;
        columns[j]->alloc_size = alloc_size;
        columns[j]->refcount = 1;
        if (type == CT_STRING) {
            dtmalloc_g(strbufs[j], StrBuf, 1);
            dtmalloc_g(strbufs[j]->buf, char, nrows * 10);
            strbufs[j]->ptr = 0;
            strbufs[j]->size = nrows * 10;
            total_alloc_size += sizeof(StrBuf) + nrows * 10;
        }
        total_alloc_size += alloc_size + sizeof(Column);
        j++;
    }

    dtmalloc_g(dt, DataTable, 1);
    dt->nrows = (int64_t) nrows;
    dt->ncols = ncols - ndrop;
    dt->rowmapping = NULL;
    dt->columns = columns;
    total_alloc_size += sizeof(DataTable);
    return total_alloc_size;

  fail:
    if (columns) {
        for (int i = 0; i < ncols - ndrop; i++) column_decref(columns[i]);
    }
    dtfree(columns);
    return 0;
}


void setFinalNrow(size_t nrows) {
    for (int i = 0, j = 0; i < ncols; i++) {
        int type = types[i];
        if (type == CT_DROP)
            continue;
        Column *col = dt->columns[j];
        if (col->stype) {} // if a column was already finalized, skip it
        if (type == CT_STRING) {
            assert(strbufs[j] != NULL);
            void *final_ptr = (void*) strbufs[j]->buf;
            strbufs[j]->buf = NULL;  // take ownership of the pointer
            size_t curr_size = strbufs[j]->ptr;
            size_t padding = ((8 - (curr_size & 7)) & 7) + 4;
            size_t offoff = curr_size + padding;
            size_t offs_size = 4 * nrows;
            size_t final_size = offoff + offs_size;
            final_ptr = realloc(final_ptr, final_size);
            memset(final_ptr + curr_size, 0xFF, padding);
            memcpy(final_ptr + offoff, col->data, offs_size);
            ((int32_t*)add_ptr(final_ptr, offoff))[-1] = -1;
            free(col->data);
            col->data = final_ptr;
            col->meta = malloc(sizeof(VarcharMeta));
            col->stype = colType_to_stype[type];
            col->alloc_size = final_size;
            ((VarcharMeta*) col->meta)->offoff = (int64_t) offoff;
        } else if (type > 0) {
            size_t new_size = stype_info[colType_to_stype[type]].elemsize * nrows;
            col->data = realloc(col->data, new_size);
            col->stype = colType_to_stype[type];
            col->alloc_size = new_size;
        }
        j++;
    }
    dt->nrows = (int64_t) nrows;
}


void reallocColType(int colidx, colType newType) {
    Column *col = dt->columns[colidx];
    size_t new_alloc_size = colTypeSizes[newType] * (size_t)dt->nrows;
    col->data = realloc(col->data, new_alloc_size);
    col->stype = ST_VOID; // Mark the column as not finalized
    col->alloc_size = new_alloc_size;
    if (newType == CT_STRING) {
        strbufs[colidx] = malloc(sizeof(StrBuf));
        strbufs[colidx]->buf = malloc(new_alloc_size * 4);
        strbufs[colidx]->ptr = 0;
        strbufs[colidx]->size = new_alloc_size * 4;
    }
}


void pushBuffer(ThreadLocalFreadParsingContext *ctx)
{
    const void *restrict buff8 = ctx->buff8;
    const void *restrict buff4 = ctx->buff4;
    const void *restrict buff1 = ctx->buff1;
    const char *restrict anchor = ctx->anchor;
    int nrows = (int) ctx->nRows;
    size_t row0 = ctx->DTi;

    int i = 0;  // index within the `types` and `sizes`
    int j = 0;  // index within `dt->columns`, `buff` and `strbufs`
    int off8 = 0, off4 = 0, off1 = 0;  // offsets within the buffers
    int rowCount8 = (int) ctx->rowSize8 / 8;
    int rowCount4 = (int) ctx->rowSize4 / 4;
    int rowCount1 = (int) ctx->rowSize1;

    for (; i < ncols; i++) {
        if (types[i] == CT_DROP)
            continue;
        Column *col = dt->columns[j];

        if (types[i] == CT_STRING) {
            assert(strbufs[j] != NULL);
            const lenOff *lenoffs = (const lenOff*)buff8 + off8;
            size_t slen = 0;  // total length of all strings to be written
            for (int n = 0; n < nrows; n++) {
                int len = lenoffs->len;
                slen += (len < 0)? 0 : (size_t)len;
                lenoffs += rowCount8;
            }
            size_t off = strbufs[j]->ptr;
            size_t size = strbufs[j]->size;
            if (size < slen + off) {
                float g = ((float)dt->nrows) / (row0 + (size_t)nrows);
                size_t newsize = (size_t)((slen + off) * max_f4(1.05f, g));
                strbufs[j]->buf = realloc(strbufs[j]->buf, (size_t)newsize);
                strbufs[j]->size = newsize;
            }
            lenoffs = (const lenOff*)buff8 + off8;
            int32_t *destptr = ((int32_t*) col->data) + row0;
            for (int n = 0; n < nrows; n++) {
                int32_t  len  = lenoffs->len;
                uint32_t aoff = lenoffs->off;
                if (len < 0) {
                    assert(len == NA_LENOFF);
                    *destptr = (int32_t)(-off - 1);
                } else {
                    if (len > 0) {
                        memcpy(strbufs[j]->buf + off, anchor + aoff, (size_t)len);
                        off += (size_t)len;
                    }
                    // TODO: handle the case when `off` is too large for int32
                    *destptr = (int32_t)(off + 1);
                }
                lenoffs += rowCount8;
                destptr++;
            }
            strbufs[j]->ptr = off;

        } else if (types[i] > 0) {
            size_t elemsize = (size_t) sizes[i];
            if (elemsize == 8) {
                const uint64_t *src = ((const uint64_t*) buff8) + off8;
                uint64_t *dest = ((uint64_t*) col->data) + row0;
                for (int r = 0; r < nrows; r++) {
                    *dest = *src;
                    src += rowCount8;
                    dest++;
                }
            } else
            if (elemsize == 4) {
                const uint32_t *src = ((const uint32_t*) buff4) + off4;
                uint32_t *dest = ((uint32_t*) col->data) + row0;
                for (int r = 0; r < nrows; r++) {
                    *dest = *src;
                    src += rowCount4;
                    dest++;
                }
            } else
            if (elemsize == 1) {
                const uint8_t *src = ((const uint8_t*) buff1) + off1;
                uint8_t *dest = ((uint8_t*) col->data) + row0;
                for (int r = 0; r < nrows; r++) {
                    *dest = *src;
                    src += rowCount1;
                    dest++;
                }
            }
        }
        off8 += (sizes[i] == 8);
        off4 += (sizes[i] == 4);
        off1 += (sizes[i] == 1);
        j++;
    }
}


void progress(double percent/*[0,1]*/, double ETA/*secs*/)
{
    (void)percent;
    (void)ETA;
}


__attribute__((format(printf, 1, 2)))
void DTPRINT(const char *format, ...) {
    va_list args;
    va_start(args, format);
    char *msg;
    if (strcmp(format, "%s") == 0) {
        msg = va_arg(args, char*);
    } else {
        msg = (char*) alloca(2001);
        vsnprintf(msg, 2000, format, args);
    }
    va_end(args);
    PyObject_CallMethod(freader, "_vlog", "O", PyUnicode_FromString(msg));
}
