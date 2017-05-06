#include <assert.h>
#include <string.h>  // memcpy
#include "fread.h"
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
    int32_t ptr;
    int32_t size;
} StrBuf;

// Forward declarations
void cleanup_fread_session(freadMainArgs *frargs);



// Python FReader object, which holds specifications for the current reader
// logic. This reference is non-NULL when fread() is running; and serves as a
// lock preventing from running multiple fread() instances.
static PyObject *freader = NULL;

// DataTable being constructed.
static DataTable *dt = NULL;

static PyObject *colNamesList = NULL;

static char *filename = NULL;
static char *input = NULL;
static char **na_strings = NULL;

static ssize_t ncols = 0;
static int8_t *types = NULL;
static int8_t *sizes = NULL;
static StrBuf **strbufs = NULL;


/**
 * Python wrapper around `freadMain()`. This function extracts the arguments
 * from the provided :class:`FReader` python object, converts them into the
 * `freadMainArgs` structure, and then passes that structure to the `freadMain`
 * function.
 */
PyObject* freadPy(PyObject *self, PyObject *args)
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
    frargs.sep = TOCHAR(ATTR(freader, "separator"), 0);
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
    frargs.nth = 1;
    frargs.warningsAreErrors = 0;
    if (frargs.nrowLimit < 0)
        frargs.nrowLimit = LONG_MAX;

    frargs.freader = freader;
    Py_INCREF(freader);

    int res = freadMain(frargs);
    if (!res) goto fail;

    DataTable_PyObject *pydt = pyDataTable_from_DataTable(dt);
    if (pydt == NULL) goto fail;
    cleanup_fread_session(&frargs);
    return (PyObject*) pydt;

  fail:
    datatable_dealloc(dt);
    cleanup_fread_session(&frargs);
    return NULL;
}


void cleanup_fread_session(freadMainArgs *frargs) {
    free(filename); filename = NULL;
    free(input); input = NULL;
    if (frargs) {
        if (na_strings) {
            char **ptr = na_strings;
            while (*ptr++) free(*ptr);
            free(na_strings);
        }
        Py_XDECREF(frargs->freader);
    }
    for (size_t i = 0; i < ncols; i++) {
        free(strbufs[i]);
    }
    free(strbufs); strbufs = NULL;
    Py_XDECREF(freader);
    Py_XDECREF(colNamesList);
    dt = NULL;
    freader = NULL;
    colNamesList = NULL;
}



_Bool userOverride(int8_t *types_, lenOff *colNames, const char *anchor, int ncols_) {
    colNamesList = PyTuple_New(ncols_);
    for (int i = 0; i < ncols_; i++) {
        lenOff ocol = colNames[i];
        PyObject *col = PyUnicode_FromStringAndSize(anchor + ocol.off, ocol.len);
        PyTuple_SET_ITEM(colNamesList, i, col);
    }
    PyObject_SetAttrString(freader, "_colnames", colNamesList);

    return 1;  // continue reading the file
}


/**
 * Allocate memory for the datatable being read
 */
size_t allocateDT(int8_t *types_, int8_t *sizes_, int ncols_, int ndrop,
                  uint64_t nrows) {
    types = types_;
    sizes = sizes_;
    ncols = (ssize_t) ncols_;

    if (nrows > INTPTR_MAX) {
        PyErr_Format(PyExc_ValueError,
            "Unable to create DataTable with %llu rows: current platform "
            "supports at most %zd rows", nrows, INTPTR_MAX);
        return 0;
    }

    Column **columns = calloc(sizeof(Column*), (size_t)(ncols - ndrop));
    strbufs = (StrBuf**) calloc(sizeof(StrBuf*), (size_t)(ncols));
    if (columns == NULL || strbufs == NULL) return 0;

    size_t total_alloc_size = sizeof(Column*) * (size_t)(ncols - ndrop) +
                              sizeof(StrBuf*) * (size_t)(ncols - ndrop);
    for (int i = 0, j = 0; i < ncols; i++) {
        int8_t type = types[i];
        if (type == CT_DROP)
            continue;
        size_t alloc_size = colTypeSizes[type] * (size_t)nrows;
        columns[j] = malloc(sizeof(Column));
        columns[j]->data = malloc(alloc_size);
        columns[j]->mtype = MT_DATA;
        columns[j]->stype = colType_to_stype[type];
        columns[j]->meta = NULL;
        columns[j]->alloc_size = alloc_size;
        assert(stype_info[columns[j]->stype].elemsize == sizes[i]);
        if (type == CT_STRING) {
            strbufs[j] = malloc(sizeof(StrBuf));
            if (strbufs[j] == NULL) return 0;
            strbufs[j]->buf = malloc(nrows*10);
            if (strbufs[j]->buf == NULL) return 0;
            strbufs[j]->ptr = 0;
            strbufs[j]->size = nrows*10;
            total_alloc_size += sizeof(StrBuf) + nrows * 10;
        }
        j++;
        total_alloc_size += alloc_size + sizeof(Column);
    }

    dt = malloc(sizeof(DataTable));
    dt->nrows = (ssize_t) nrows;
    dt->ncols = ncols - ndrop;
    dt->source = NULL;
    dt->rowmapping = NULL;
    dt->columns = columns;
    total_alloc_size += sizeof(DataTable);
    return total_alloc_size;
}


void setFinalNrow(uint64_t nrows) {
    for (int i = 0; i < dt->ncols; i++) {
        Column *col = dt->columns[i];
        if (col->stype == ST_STRING_I4_VCHAR) {
            assert(strbufs[i] != NULL);
            int32_t curr_size = strbufs[i]->ptr;
            int32_t padding = (8 - (curr_size & 7)) & 7;
            int32_t offoff = curr_size + padding;
            int32_t offs_size = 4 * (int32_t)nrows;
            int32_t final_size = offoff + offs_size;
            unsigned char *final_ptr = realloc(strbufs[i]->buf, (size_t)final_size);
            memset(final_ptr + curr_size, 0xFF, (size_t)padding);
            memcpy(final_ptr + offoff, col->data, (size_t)offs_size);
            free(col->data);
            col->data = final_ptr;
            strbufs[i]->buf = NULL;  // col->data now "owns" this pointer
            col->meta = malloc(sizeof(VarcharMeta));
            ((VarcharMeta*)(col->meta))->offoff = (int64_t) offoff;
        } else {
            size_t new_size = stype_info[col->stype].elemsize * (size_t)nrows;
            col->data = realloc(col->data, new_size);
        }
    }
    dt->nrows = (ssize_t) nrows;
}


void reallocColType(int colidx, colType newType) {
    Column *col = dt->columns[colidx];
    size_t new_alloc_size = colTypeSizes[newType] * (size_t)dt->nrows;
    col->data = realloc(col->data, new_alloc_size);
    if (newType == CT_STRING) {
        strbufs[colidx] = malloc(sizeof(StrBuf));
        strbufs[colidx]->buf = malloc(new_alloc_size * 4);
        strbufs[colidx]->ptr = 0;
        strbufs[colidx]->size = new_alloc_size * 4;
    }
}


void pushBuffer(const void *buff, const char *anchor, int nrows,
                int64_t row0, int rowSize,
                int nStringCols, int nNonStringCols)
{
    int i = 0;  // index within the `types` and `sizes`
    int j = 0;  // index within `dt->columns`, `buff` and `strbufs`
    int ptr = 0;  // offset within the buffer of the current column
    for (; i < ncols; i++) {
        if (types[i] == CT_DROP)
            continue;
        Column *col = dt->columns[j];

        if (types[i] == CT_STRING) {
            const void* srcptr = buff + ptr;
            int32_t slen = 0;  // total length of all strings to be written
            for (int n = 0; n < nrows; n++) {
                // Warning: potentially misaligned pointer access here
                int len = (*(const lenOff*)srcptr).len;
                slen += (len == NA_LENOFF)? 0 : len;
                srcptr += rowSize;
            }
            assert(strbufs[j] != NULL);
            int32_t off = strbufs[j]->ptr;
            int32_t size = strbufs[j]->size;
            if (size < slen + off) {
                float g = ((float)dt->nrows) / (row0 + nrows);
                int32_t newsize = (int32_t)((slen + off) * max_f4(1.05f, g));
                strbufs[j]->buf = realloc(strbufs[j]->buf, (size_t)newsize);
                strbufs[j]->size = newsize;
            }
            srcptr = buff + ptr;
            int32_t *destptr = col->data + row0;
            for (int n = 0; n < nrows; n++) {
                int32_t  len  = ((const lenOff*)srcptr)->len;
                uint32_t aoff = ((const lenOff*)srcptr)->off;
                if (len < 0) {
                    assert(len == NA_LENOFF);
                    *destptr++ = -off - 1;
                } else {
                    memcpy(strbufs[j]->buf + off, anchor + aoff, (size_t)len);
                    off += len;
                    *destptr++ = off + 1;
                }
                srcptr += rowSize;
            }
            strbufs[j]->ptr = off;

        } else if (types[i] > 0) {
            size_t elemsize = (size_t) sizes[i];
            void *destptr = col->data + (size_t)row0 * elemsize;
            const void *srcptr = buff + ptr;
            for (int r = 0; r < nrows; r++) {
                memcpy(destptr, srcptr, elemsize);
                srcptr += rowSize;
                destptr += elemsize;
            }
        }
        ptr += sizes[i];
        j++;
    }
}


void progress(double percent/*[0,1]*/, double ETA/*secs*/) {}

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
