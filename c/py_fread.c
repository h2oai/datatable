#include <string.h>  // memcpy
#include "fread.h"
#include "myassert.h"
#include "utils.h"
#include "py_datatable.h"
#include "py_encodings.h"
#include "py_utils.h"


static const SType colType_to_stype[NUMTYPE] = {
    ST_VOID,
    ST_BOOLEAN_I1,
    ST_INTEGER_I4,
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

// List of column names for the final DataTable
static PyObject *colNamesList = NULL;

static char *filename = NULL;
static char *input = NULL;
static char *skipstring = NULL;
static char **na_strings = NULL;

// ncols -- number of fields in the CSV file.
// ndtcols -- number of columns in the output DataTable (which is `ncols` minus
//     the number of dropped columns).
// nstrcols -- number of string columns in the output DataTable.
static size_t ncols = 0;
static size_t ndtcols = 0;
static size_t nstrcols = 0;

// types -- array of types for each field in the input file. Length = `ncols`
// sizes -- array of byte sizes for each field. Length = `ncols`.
// Both of these arrays are borrowed references and are valid only for the
// duration of the parse. Should not be freed.
static int8_t *types = NULL;
static int8_t *sizes = NULL;

// strbufs -- array of auxiliary values for all string columns in the output
//     DataTable (length = `nstrcols`). Each `StrBuf` contains the following
//     values:
//     .buf -- memory region where all strings data is stored, back-to-back.
//     .size -- allocation size of `.buf`.
//     .ptr -- the amount of buffer that was filled so far, i.e. offset within
//         the buffer of the first unwritten byte.
//     .idx8 -- index of this column within the `buff8` memory buffer.
static StrBuf *strbufs = NULL;



//------------------------------------------------------------------------------

/**
 * Python wrapper around `freadMain()`. This function extracts the arguments
 * from the provided :class:`FReader` python object, converts them into the
 * `freadMainArgs` structure, and then passes that structure to the `freadMain`
 * function.
 */
PyObject* freadPy(UU, PyObject *args)
{
    PyObject *tmp1 = NULL, *tmp2 = NULL, *tmp3 = NULL;
    if (freader != NULL) {
        PyErr_SetString(PyExc_RuntimeError,
            "Cannot run multiple instances of fread() in-parallel.");
        return NULL;
    }
    if (!PyArg_ParseTuple(args, "O:fread", &freader))
        return NULL;

    Py_INCREF(freader);

    freadMainArgs *frargs = NULL;
    dtmalloc_g(frargs, freadMainArgs, 1);

    // filename & input are borrowed references
    filename = TOSTRING(ATTR(freader, "filename"), &tmp1);
    input = TOSTRING(ATTR(freader, "text"), &tmp2);
    skipstring = TOSTRING(ATTR(freader, "skip_to_string"), &tmp3);
    na_strings = TOSTRINGLIST(ATTR(freader, "na_strings"), NULL);

    frargs->filename = filename;
    frargs->input = input;
    frargs->sep = TOCHAR(ATTR(freader, "sep"), 0);
    frargs->dec = '.';
    frargs->quote = '"';
    frargs->nrowLimit = TOINT64(ATTR(freader, "max_nrows"), 0);
    frargs->skipNrow = TOINT64(ATTR(freader, "skip_lines"), 0);
    frargs->skipString = skipstring;
    frargs->header = TOBOOL(ATTR(freader, "header"), NA_BOOL8);
    frargs->verbose = TOBOOL(ATTR(freader, "verbose"), 0);
    frargs->NAstrings = (const char* const*) na_strings;
    frargs->stripWhite = 1;
    frargs->skipEmptyLines = 1;
    frargs->fill = TOBOOL(ATTR(freader, "fill"), 0);
    frargs->showProgress = TOBOOL(ATTR(freader, "show_progress"), 0);
    frargs->nth = 0;
    frargs->warningsAreErrors = 0;
    if (frargs->nrowLimit < 0)
        frargs->nrowLimit = LONG_MAX;

    frargs->freader = freader;
    Py_INCREF(freader);

    int res = freadMain(*frargs);
    if (!res) goto fail;

    PyObject *pydt = pydt_from_dt(dt);
    if (pydt == NULL) goto fail;
    Py_XDECREF(tmp1);
    Py_XDECREF(tmp2);
    Py_XDECREF(tmp3);
    cleanup_fread_session(frargs);
    return pydt;

  fail:
    datatable_dealloc(dt);
    cleanup_fread_session(frargs);
    dtfree(frargs);
    return NULL;
}



void cleanup_fread_session(freadMainArgs *frargs) {
    ncols = 0;
    ndtcols = 0;
    nstrcols = 0;
    types = NULL;
    sizes = NULL;
    if (frargs) {
        if (na_strings) {
            char **ptr = na_strings;
            while (*ptr++) dtfree(*ptr);
            dtfree(na_strings);
        }
        pyfree(frargs->freader);
    }
    if (strbufs) {
        for (size_t i = 0; i < nstrcols; i++) {
            dtfree(strbufs[i].buf);
        }
        dtfree(strbufs);
    }
    pyfree(freader);
    pyfree(colNamesList);
    dt = NULL;
}



_Bool userOverride(int8_t *types_, lenOff *colNames, const char *anchor,
                   int ncols_)
{
    types = types_;
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
 * Allocate memory for the DataTable that is being constructed.
 */
size_t allocateDT(int8_t *types_, int8_t *sizes_, int ncols_, int ndrop_,
                  size_t nrows)
{
    Column **columns = NULL;
    types = types_;
    sizes = sizes_;
    size_t total_alloc_size = 0;

    int reallocate = (ncols > 0);
    if (reallocate) {
        assert(dt != NULL && strbufs == NULL);
        assert(ncols == (size_t)ncols_ && nstrcols == 0);
        columns = dt->columns;
    } else {
        assert(dt == NULL && strbufs == NULL);
        assert(ncols == 0 && ndtcols == 0 && nstrcols == 0);
        ncols = (size_t) ncols_;
        ndtcols = (size_t)(ncols_ - ndrop_);
        dtcalloc_g(columns, Column*, ndtcols + 1);
        columns[ndtcols] = NULL;
        total_alloc_size += sizeof(Column*) * ndtcols;
    }

    nstrcols = 0;
    for (int i = 0; i < ncols_; i++)
        nstrcols += (types[i] == CT_STRING);
    dtcalloc_g(strbufs, StrBuf, nstrcols);

    int i = 0;
    size_t j = 0;
    size_t k = 0;
    size_t off8 = 0;
    for (i = 0; i < ncols_; i++) {
        int8_t type = types[i];
        if (type == CT_DROP) continue;
        if (type < 0) { j++; continue; }
        assert(j < ndtcols);

        if (reallocate) column_decref(columns[j]);
        columns[j] = make_column(colType_to_stype[type], nrows);
        if (columns[j] == NULL) goto fail;
        // column's stype will be 0 until the column is finalized
        columns[j]->stype = 0;

        if (type == CT_STRING) {
            assert(k < nstrcols);
            size_t alloc_size = nrows * 10;
            dtmalloc_g(strbufs[k].buf, char, alloc_size);
            strbufs[k].ptr = 0;
            strbufs[k].size = alloc_size;
            strbufs[k].idx8 = off8;
            strbufs[k].numuses = 0;
            total_alloc_size += alloc_size;
            k++;
        }
        total_alloc_size += columns[j]->alloc_size + sizeof(Column);
        off8 += (sizes[i] == 8);
        j++;
    }
    assert((size_t)i == ncols && j == ndtcols && k == nstrcols);

    if (!reallocate) {
        dt = make_datatable((int64_t)nrows, columns);
        if (dt == NULL) goto fail;
        total_alloc_size += sizeof(DataTable);
    }
    return total_alloc_size;

  fail:
    if (columns) {
        for (j = 0; j < ndtcols; j++) column_decref(columns[j]);
        dtfree(columns);
    }
    if (strbufs) {
        for (k = 0; k < nstrcols; k++) dtfree(strbufs[k].buf);
        dtfree(strbufs);
    }
    return 0;
}



void setFinalNrow(size_t nrows) {
    size_t i, j, k;
    for (i = j = k = 0; i < ncols; i++) {
        int type = types[i];
        if (type == CT_DROP) continue;
        Column *col = dt->columns[j];
        if (col->stype) {} // if a column was already finalized, skip it
        if (type == CT_STRING) {
            assert(strbufs[k].numuses == 0);
            void *final_ptr = (void*) strbufs[k].buf;
            strbufs[k].buf = NULL;  // take ownership of the pointer
            size_t curr_size = strbufs[k].ptr;
            size_t padding = ((8 - (curr_size & 7)) & 7) + 4;
            size_t offoff = curr_size + padding;
            size_t offs_size = 4 * nrows;
            size_t final_size = offoff + offs_size;
            dtrealloc_g(final_ptr, void, final_size);
            memset(add_ptr(final_ptr, curr_size), 0xFF, padding);
            memcpy(add_ptr(final_ptr, offoff), col->data, offs_size);
            ((int32_t*)add_ptr(final_ptr, offoff))[-1] = -1;
            dtfree(col->data);
            col->data = final_ptr;
            col->stype = colType_to_stype[type];
            col->alloc_size = final_size;
            ((VarcharMeta*) col->meta)->offoff = (int64_t) offoff;
            k++;
        } else if (type > 0) {
            size_t new_size = stype_info[colType_to_stype[type]].elemsize * nrows;
            dtrealloc_g(col->data, void, new_size);
            col->stype = colType_to_stype[type];
            col->alloc_size = new_size;
        }
        j++;
    }
    dt->nrows = (int64_t) nrows;
    dtfree(strbufs);
    return;
  fail:
    printf("setFinalNrow() failed!\n");
}


void prepareThreadContext(ThreadLocalFreadParsingContext *ctx)
{
    ctx->strbufs = NULL;
    dtcalloc_g(ctx->strbufs, StrBuf, nstrcols);
    for (size_t k = 0; k < nstrcols; k++) {
        dtmalloc_g(ctx->strbufs[k].buf, char, 4096);
        ctx->strbufs[k].size = 4096;
        ctx->strbufs[k].ptr = 0;
        ctx->strbufs[k].idx8 = strbufs[k].idx8;
    }
    return;

  fail:
    printf("prepareThreadContext() failed\n");
    for (size_t k = 0; k < nstrcols; k++) dtfree(ctx->strbufs[k].buf);
    dtfree(ctx->strbufs);
    *(ctx->stopTeam) = 1;
}


void postprocessBuffer(ThreadLocalFreadParsingContext *ctx)
{
    StrBuf *ctx_strbufs = ctx->strbufs;
    const unsigned char *anchor = (const unsigned char*) ctx->anchor;
    size_t nrows = ctx->nRows;
    lenOff *restrict const lenoffs = (lenOff *restrict) ctx->buff8;
    int rowCount8 = (int) ctx->rowSize8 / 8;

    for (size_t k = 0; k < nstrcols; k++) {
        assert(ctx_strbufs != NULL);

        lenOff *restrict lo = lenoffs + ctx_strbufs[k].idx8;
        unsigned char *strdest = (unsigned char*) ctx_strbufs[k].buf;
        int32_t off = 1;
        size_t bufsize = ctx_strbufs[k].size;
        for (size_t n = 0; n < nrows; n++) {
            int32_t len  = lo->len;
            if (len > 0) {
                size_t zlen = (size_t) len;
                if (bufsize < zlen * 3 + (size_t)off) {
                    bufsize = bufsize * 2 + zlen * 3;
                    dtrealloc_g(strdest, unsigned char, bufsize);
                    ctx_strbufs[k].buf = (char*) strdest;
                    ctx_strbufs[k].size = bufsize;
                }
                const unsigned char *src = anchor + lo->off;
                unsigned char *dest = strdest + off - 1;
                if (is_valid_utf8(src, zlen)) {
                    memcpy(dest, src, zlen);
                    off += zlen;
                    lo->off = off;
                } else {
                    int newlen = decode_windows1252(src, len, dest);
                    assert(newlen > 0);
                    off += (size_t) newlen;
                    lo->off = off;
                }
            } else if (len == 0) {
                lo->off = off;
            } else {
                assert(len == NA_LENOFF);
                lo->off = -off;
            }
            lo += rowCount8;
        }
        ctx_strbufs[k].ptr = (size_t)(off - 1);
    }
    return;
  fail:
    printf("postprocessBuffer() failed\n");
    *(ctx->stopTeam) = 1;
}


void orderBuffer(ThreadLocalFreadParsingContext *ctx)
{
    StrBuf *ctx_strbufs = ctx->strbufs;
    StrBuf *glb_strbufs = strbufs;
    for (size_t k = 0; k < nstrcols; k++) {
        size_t sz = ctx_strbufs[k].ptr;
        size_t ptr = glb_strbufs[k].ptr;
        // If we need to write more than the size of the available buffer, the
        // buffer has to grow. Check documentation for `StrBuf.numuses` in
        // `py_fread.h`.
        while (ptr + sz > glb_strbufs[k].size) {
            size_t newsize = (size_t)((ptr + sz) * 2);
            int old = 0;
            // (1) wait until no other process is writing into the buffer
            while (glb_strbufs[k].numuses > 0)
                /* wait until .numuses == 0 (all threads finished writing) */;
            // (2) make `numuses` negative, indicating that no other thread may
            // initiate a memcopy operation for now.
            #pragma omp atomic capture
            { old = glb_strbufs[k].numuses; glb_strbufs[k].numuses -= 1000000; }
            // (3) The only case when `old != 0` is if another thread started
            // memcopy operation in-between statements (1) and (2) above. In
            // that case we restore the previous value of `numuses` and repeat
            // the loop.
            // Otherwise (and it is the most common case) we reallocate the
            // buffer and only then restore the `numuses` variable.
            if (old == 0) {
                dtrealloc_g(glb_strbufs[k].buf, char, newsize);
                glb_strbufs[k].size = newsize;
            }
            #pragma omp atomic update
            glb_strbufs[k].numuses += 1000000;
        }
        ctx_strbufs[k].ptr = ptr;
        glb_strbufs[k].ptr = ptr + sz;
    }
    return;
  fail:
    printf("orderBuffer() failed");
    *(ctx->stopTeam) = 1;
}


void pushBuffer(ThreadLocalFreadParsingContext *ctx)
{
    StrBuf *restrict ctx_strbufs = ctx->strbufs;
    StrBuf *restrict glb_strbufs = strbufs;
    const void *restrict buff8 = ctx->buff8;
    const void *restrict buff4 = ctx->buff4;
    const void *restrict buff1 = ctx->buff1;
    int nrows = (int) ctx->nRows;
    size_t row0 = ctx->DTi;

    size_t i = 0;  // index within the `types` and `sizes`
    size_t j = 0;  // index within `dt->columns`, `buff` and `strbufs`
    int off8 = 0, off4 = 0, off1 = 0;  // offsets within the buffers
    int rowCount8 = (int) ctx->rowSize8 / 8;
    int rowCount4 = (int) ctx->rowSize4 / 4;
    int rowCount1 = (int) ctx->rowSize1;

    size_t k = 0;
    for (; i < ncols; i++) {
        if (types[i] == CT_DROP) continue;
        Column *col = dt->columns[j];

        if (types[i] == CT_STRING) {
            size_t idx8 = ctx_strbufs[k].idx8;
            size_t ptr = ctx_strbufs[k].ptr;
            const lenOff *restrict lo = add_constptr(buff8, idx8 * 8);
            size_t sz = (size_t) abs(lo[(nrows - 1)*rowCount8].off) - 1;

            int done = 0;
            while (!done) {
                int old;
                #pragma omp atomic capture
                old = glb_strbufs[k].numuses++;
                if (old >= 0) {
                    memcpy(glb_strbufs[k].buf + ptr, ctx_strbufs[k].buf, sz);
                    done = 1;
                }
                #pragma omp atomic update
                glb_strbufs[k].numuses--;
            }

            int32_t* dest = ((int32_t*) col->data) + row0;
            int32_t iptr = (int32_t) ptr;
            for (int n = 0; n < nrows; n++) {
                int32_t off = lo->off;
                *dest++ = (off < 0)? off - iptr : off + iptr;
                lo += rowCount8;
            }
            k++;

        } else if (types[i] > 0) {
            int8_t elemsize = sizes[i];
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
    (void)ETA;
    PyObject_CallMethod(freader, "_progress", "d", percent);
}


void freeThreadContext(ThreadLocalFreadParsingContext *ctx)
{
    if (ctx->strbufs) {
        for (size_t k = 0; k < nstrcols; k++) {
            dtfree(ctx->strbufs[k].buf);
        }
        dtfree(ctx->strbufs);
    }
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
