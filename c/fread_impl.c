#include <string.h>  // memcpy
#include "fread.h"
#include "py_datatable.h"
#include "py_utils.h"

static const size_t colTypeSizes[NUMTYPE] = {0, 1, 4, 8, 8, 8};
static const SType colType_to_stype[NUMTYPE] = {
    ST_VOID,
    ST_BOOLEAN_I1,
    ST_INTEGER_I4,
    ST_INTEGER_I8,
    ST_REAL_F8,
    ST_STRING_I4_VCHAR,
};

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

static char **strbufs = NULL;
static int32_t *strbuf_ptrs = NULL;
static int32_t *strbuf_sizes = NULL;
static size_t strbuf_count = 0;


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
    frargs.fill = 0;
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
    if (strbuf_count) {
        for (size_t i = 0; i < strbuf_count; i++) free(strbufs[i]);
        free(strbuf_sizes); strbuf_sizes = NULL;
        free(strbuf_ptrs); strbuf_ptrs = NULL;
        free(strbufs); strbufs = NULL;
        strbuf_count = 0;
    }
    Py_XDECREF(freader);
    Py_XDECREF(colNamesList);
    dt = NULL;
    freader = NULL;
    colNamesList = NULL;
}



_Bool userOverride(int8_t *types, lenOff *colNames, const char *anchor, int ncols) {

    colNamesList = PyTuple_New(ncols);
    for (int i = 0; i < ncols; i++) {
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
size_t allocateDT(int8_t *types, int ncols, int ndrop, uint64_t nrows) {
    if (nrows > INTPTR_MAX) {
        PyErr_Format(PyExc_ValueError,
            "Unable to create DataTable with %llu rows: current platform "
            "supports at most %zd rows", nrows, INTPTR_MAX);
        return 0;
    }

    Column **columns = calloc(sizeof(Column*), (size_t)(ncols - ndrop));
    if (columns == NULL) return 0;

    size_t n_string_cols = 0;
    size_t total_alloc_size = 0;
    for (int i = 0, j = 0; i < ncols; i++) {
        int8_t type = types[i];
        if (type == CT_DROP)
            continue;
        size_t alloc_size = colTypeSizes[type] * (size_t)nrows;
        columns[j]->data = malloc(alloc_size);
        columns[j]->mtype = MT_DATA;
        columns[j]->stype = colType_to_stype[type];
        columns[j]->meta = NULL;
        columns[j]->alloc_size = alloc_size;
        if (type == CT_STRING) n_string_cols++;
        j++;
        total_alloc_size += alloc_size;
    }
    if (n_string_cols > 0) {
        strbufs = calloc(sizeof(char*), n_string_cols);
        strbuf_sizes = calloc(sizeof(int32_t), n_string_cols);
        strbuf_ptrs = calloc(sizeof(int32_t), n_string_cols);
        strbuf_count = n_string_cols;
    }

    dt = malloc(sizeof(DataTable));
    dt->nrows = (ssize_t) nrows;
    dt->ncols = ncols - ndrop;
    dt->source = NULL;
    dt->rowmapping = NULL;
    dt->columns = columns;
    return 0;
}


void setFinalNrow(uint64_t nrows) {
    int k = 0;
    for (int i = 0; i < dt->ncols; i++) {
        Column *col = dt->columns[i];
        if (col->stype == ST_STRING_I4_VCHAR) {
            int32_t curr_size = strbuf_ptrs[k];
            int32_t padding = (8 - (curr_size & 7)) & 7;
            int32_t offoff = curr_size + padding;
            int32_t offs_size = 4 * (int32_t)nrows;
            int32_t final_size = offoff + offs_size;
            unsigned char *final_ptr = realloc(strbufs[k], (size_t)final_size);
            memset(final_ptr + curr_size, 0xFF, padding);
            memcpy(final_ptr + offoff, col->data, offs_size);
            free(col->data);
            col->data = final_ptr;
            strbufs[k] = NULL;  // col->data now "owns" this pointer
            col->meta = malloc(sizeof(VarcharMeta));
            ((VarcharMeta*)(col->meta))->offoff = (int64_t) offoff;
            k++;
        } else {
            size_t new_size = stype_info[col->stype].elemsize * (size_t)nrows;
            realloc(col->data, new_size);
        }
    }
    dt->nrows = (ssize_t) nrows;
}


void reallocColType(int colidx, colType newType) {
    size_t new_alloc_size = colTypeSizes[newType] * (size_t)dt->nrows;
    realloc(dt->columns[colidx]->data, new_alloc_size);
}


void pushBuffer(int8_t *types, int ncols, void **buff, const char *anchor,
                int nStringCols, int nNonStringCols, uint32_t nRows,
                uint64_t ansi)
{
    int i = 0;  // index within the `types`
    int j = 0;  // index within `dt->columns` and `buff`
    int k = 0;  // index within `strbufs` & others
    for (; i < ncols; i++) {
        if (types[i] == CT_DROP)
            continue;
        Column *col = dt->columns[j];
        if (types[i] == CT_STRING) {
            lenOff* relstrings = buff[j];
            int32_t off = strbuf_ptrs[k];
            int32_t size = strbuf_sizes[k];
            int32_t slen = 0;  // total length of all strings to be written
            for (uint32_t n = 0; n < nRows; n++) {
                int32_t len = relstrings[n].len;
                slen += (len == NA_LENOFF)? 0 : len;
            }
            if (size < slen + off) {
                float g = ((float)dt->nrows) / (ansi + nRows);
                int32_t newsize = (int32_t)((slen + off) * max_f4(1.05f, g));
                strbufs[k] = realloc(strbufs[k], (size_t)newsize);
                strbuf_sizes[k] = newsize;
            }
            for (uint32_t n = 0; n < nRows; n++) {
                int32_t len = relstrings[n].len;
                if (len == NA_LENOFF) {
                    ((int32_t*)col->data)[ansi + n] = -off - 1;
                } else {
                    memcpy(strbufs[k] + off, anchor + relstrings[n].off, len);
                    off += len;
                    ((int32_t*)col->data)[ansi + n] = off + 1;
                }
            }
            strbuf_ptrs[k] = off;
            k++;
        } else {
            size_t elemsize = stype_info[col->stype].elemsize;
            memcpy(col->data + ansi * elemsize, buff[j], nRows * elemsize);
        }
        j++;
    }
}


void progress(double percent/*[0,1]*/, double ETA/*secs*/) {}


__attribute__((format(printf, 1, 2)))
void DTPRINT(const char *format, ...) {
    va_list args;
    va_start(args, format);
    static char msg[2000];
    vsnprintf(msg, 2000, format, args);
    va_end(args);
    PyObject_CallMethod(freader, "_vlog", "O", PyUnicode_FromString(msg));
}
