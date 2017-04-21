#include <string.h>  // memcpy
#include "fread.h"
#include "py_datatable.h"
#include "py_utils.h"

static const size_t colTypeSizes[NUMTYPE] = {0, 1, 4, 8, 8, 8};
static const DataSType colType_to_stype[NUMTYPE] = {
    DT_VOID,
    DT_BOOLEAN_I8,
    DT_INTEGER_I32,
    DT_INTEGER_I64,
    DT_REAL_F64,
    DT_STRING_I64_VCHAR,
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
    dt_DataTable_dealloc(dt, NULL);
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
    Column *columns = calloc(sizeof(Column), (size_t)(ncols - ndrop));
    for (int i = 0, j = 0; i < ncols; i++) {
        int8_t type = types[i];
        if (type != CT_DROP) {
            size_t alloc_size = colTypeSizes[type] * nrows;
            columns[j].data = malloc(alloc_size);
            columns[j].stype = colType_to_stype[type];
            columns[j].meta = NULL;
            columns[j].srcindex = -1;
            columns[j].mmapped = 0;
            j++;
        }
    }

    dt = malloc(sizeof(DataTable));
    dt->nrows = (int64_t) nrows;
    dt->ncols = ncols - ndrop;
    dt->source = NULL;
    dt->rowmapping = NULL;
    dt->columns = columns;
    return 0;
}


void setFinalNrow(uint64_t nrows) {
    for (int i = 0; i < dt->ncols; i++) {
        Column col = dt->columns[i];
        size_t new_size = stype_info[col.stype].elemsize * nrows;
        realloc(col.data, new_size);
    }
    dt->nrows = (int64_t) nrows;
}


void reallocColType(int col, colType newType) {
    size_t new_alloc_size = colTypeSizes[newType] * (size_t)dt->nrows;
    realloc(dt->columns[col].data, new_alloc_size);
}


void pushBuffer(int8_t *types, int ncols, void **buff, const char *anchor,
                int nStringCols, int nNonStringCols, uint32_t nRows,
                uint64_t ansi)
{
    for (int i = 0; i < ncols; i++) {
        Column col = dt->columns[i];
        size_t elemsize = stype_info[col.stype].elemsize;
        memcpy(col.data + ansi * elemsize, buff[i], nRows * elemsize);
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
