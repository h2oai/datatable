#include <string.h>  // memcpy
#include <fcntl.h>     // open
#include <unistd.h>    // close, truncate
#include <sys/mman.h>
#include "py_fread.h"
#include "fread.h"
#include "memorybuf.h"
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
    ST_REAL_F4,
    ST_REAL_F8,
    ST_REAL_F8,
    ST_REAL_F8,
    ST_STRING_I4_VCHAR,
};


// Forward declarations
static void cleanup_fread_session(freadMainArgs *frargs);



// Python FReader object, which holds specifications for the current reader
// logic. This reference is non-NULL when fread() is running; and serves as a
// lock preventing from running multiple fread() instances.
static PyObject *freader = NULL;
static PyObject *flogger = NULL;

// DataTable being constructed.
static DataTable *dt = NULL;

// These variables are handed down to `freadMain`, and are stored globally only
// because we want to free these memory buffers in the end.
static char *filename = NULL;
static char *input = NULL;
static char *targetdir = NULL;
static char *skipstring = NULL;
static char **na_strings = NULL;

// For temporary printing file names.
static char fname[1000];
static char fname2[1000];

// ncols -- number of fields in the CSV file. This field first becomes available
//     in the `userOverride()` callback, and doesn't change after that.
// nstrcols -- number of string columns in the output DataTable. This will be
//     computed within `allocateDT()` callback, and used for allocation of
//     string buffers. If the file is re-read (due to type bumps), this variable
//     will only count those string columns that need to be re-read.
// ndigits = len(str(ncols))
// verbose -- if True, then emit verbose messages during parsing.
//
static int ncols = 0;
static int nstrcols = 0;
static int ndigits = 0;
static int verbose = 0;

// types -- array of types for each field in the input file. Length = `ncols`.
// sizes -- array of byte sizes for each field. Length = `ncols`.
// Both of these arrays are borrowed references and are valid only for the
// duration of the parse. Must not be freed.
static int8_t *types = NULL;
static int8_t *sizes = NULL;



//------------------------------------------------------------------------------

/**
 * Python wrapper around `freadMain()`. This function extracts the arguments
 * from the provided :class:`FReader` python object, converts them into the
 * `freadMainArgs` structure, and then passes that structure to the `freadMain`
 * function.
 */
PyObject* pyfread(UU, PyObject *args)
{
    PyObject *tmp1 = NULL, *tmp2 = NULL, *tmp3 = NULL, *pydt = NULL;
    int retval = 0;
    if (freader != NULL || dt != NULL) {
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
    na_strings = TOSTRINGLIST(ATTR(freader, "na_strings"));
    verbose = TOBOOL(ATTR(freader, "verbose"), 0);
    flogger = ATTR(freader, "logger");
    Py_INCREF(flogger);

    frargs->filename = filename;
    frargs->input = input;
    frargs->sep = TOCHAR(ATTR(freader, "sep"), 0);
    frargs->dec = '.';
    frargs->quote = '"';
    frargs->nrowLimit = TOINT64(ATTR(freader, "max_nrows"), 0);
    frargs->skipNrow = TOINT64(ATTR(freader, "skip_lines"), 0);
    frargs->skipString = skipstring;
    frargs->header = TOBOOL(ATTR(freader, "header"), NA_BOOL8);
    frargs->verbose = verbose;
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

    retval = freadMain(*frargs);
    if (!retval) goto fail;

    pydt = pydt_from_dt(dt);
    if (pydt == NULL) goto fail;
    Py_XDECREF(tmp1);
    Py_XDECREF(tmp2);
    Py_XDECREF(tmp3);
    cleanup_fread_session(frargs);
    return pydt;

  fail:
    delete dt;
    cleanup_fread_session(frargs);
    dtfree(frargs);
    return NULL;
}


static Column* alloc_column(SType stype, size_t nrows, int j)
{
    // TODO(pasha): figure out how to use `WritableBuffer`s here
    Column *col = NULL;
    if (targetdir) {
        snprintf(fname, 1000, "%s/col%0*d", targetdir, ndigits, j);
        col = Column::new_mmap_column(stype, static_cast<int64_t>(nrows), fname);
    } else{
        col = Column::new_data_column(stype, static_cast<int64_t>(nrows));
    }
    if (col == NULL) return NULL;

    // For string columns we temporarily replace the `meta` structure with a
    // `StrBuf` which will hold auxiliary values needed for construction of
    // the column.
    if (stype_info[stype].ltype == LT_STRING) {
        dtrealloc(col->meta, StrBuf, 1);
        StrBuf *sb = (StrBuf*) col->meta;
        // Pre-allocate enough memory to hold 5-char strings in the buffer. If
        // this is not enough, we will always be able to re-allocate during the
        // run time.
        size_t alloc_size = nrows * 5;
        if (targetdir) {
            snprintf(fname, 1000, "%s/str%0*d", targetdir, ndigits, j);
            // Create new file of size `alloc_size`.
            FILE *fp = fopen(fname, "w");
            fseek(fp, (long)(alloc_size - 1), SEEK_SET);
            fputc('\0', fp);
            fclose(fp);
            // Memory-map the file.
            int fd = open(fname, O_RDWR);
            sb->buf = (char*) mmap(NULL, alloc_size, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
            if (sb->buf == MAP_FAILED) {
                printf("Memory map failed with errno %d: %s\n", errno, strerror(errno));
                return NULL;
            }
            close(fd);
        } else {
            dtmalloc(sb->buf, char, alloc_size);
        }
        sb->size = alloc_size;
        sb->ptr = 0;
        sb->idx8 = -1;  // not needed for this structure
        sb->idxdt = j;
        sb->numuses = 0;
    }
    return col;
}


Column* realloc_column(Column *col, SType stype, size_t nrows, int j)
{
    if (col != NULL && stype != col->stype()) {
        delete col;
        return alloc_column(stype, nrows, j);
    }
    if (col == NULL) {
        return alloc_column(stype, nrows, j);
    }

    size_t new_alloc_size = stype_info[stype].elemsize * nrows;
    col->mbuf->resize(new_alloc_size);
    col->nrows = (int64_t) nrows;
    return col;
}



static void cleanup_fread_session(freadMainArgs *frargs) {
    ncols = 0;
    nstrcols = 0;
    types = NULL;
    sizes = NULL;
    dtfree(targetdir);
    if (frargs) {
        if (na_strings) {
            char **ptr = na_strings;
            while (*ptr++) dtfree(*ptr);
            dtfree(na_strings);
        }
        pyfree(frargs->freader);
    }
    pyfree(freader);
    pyfree(flogger);
    dt = NULL;
}



bool userOverride(int8_t *types_, lenOff *colNames, const char *anchor,
                   int ncols_)
{
    types = types_;
    PyObject *colNamesList = PyList_New(ncols_);
    PyObject *colTypesList = PyList_New(ncols_);
    for (int i = 0; i < ncols_; i++) {
        lenOff ocol = colNames[i];
        PyObject *col =
            ocol.len > 0? PyUnicode_FromStringAndSize(anchor + ocol.off, ocol.len)
                        : PyUnicode_FromFormat("V%d", i);
        PyObject *typ = PyLong_FromLong(types[i]);
        PyList_SET_ITEM(colNamesList, i, col);
        PyList_SET_ITEM(colTypesList, i, typ);
    }
    PyObject *ret = PyObject_CallMethod(freader, "_override_columns",
                                        "OO", colNamesList, colTypesList);
    if (!ret) {
        pyfree(colTypesList);
        pyfree(colNamesList);
        return 0;
    }

    for (int i = 0; i < ncols_; i++) {
        PyObject *t = PyList_GET_ITEM(colTypesList, i);
        types[i] = (int8_t) PyLong_AsUnsignedLongMask(t);
    }

    pyfree(colTypesList);
    pyfree(colNamesList);
    pyfree(ret);
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
    nstrcols = 0;

    // First we need to estimate the size of the dataset that needs to be
    // created. However this needs to be done on first run only.
    // Also in this block we compute: `nstrcols` (will be used later in
    // `prepareThreadContext` and `postprocessBuffer`), as well as allocating
    // the `Column**` array.
    if (ncols == 0) {
        // DTPRINT("Writing the DataTable into %s\n", targetdir);
        assert(dt == NULL);
        ncols = ncols_;

        size_t alloc_size = 0;
        int i, j;
        for (i = j = 0; i < ncols; i++) {
            if (types[i] == CT_DROP) continue;
            nstrcols += (types[i] == CT_STRING);
            SType stype = colType_to_stype[types[i]];
            alloc_size += stype_info[stype].elemsize * nrows;
            if (types[i] == CT_STRING) alloc_size += 5 * nrows;
            j++;
        }
        assert(j == ncols_ - ndrop_);
        dtcalloc_g(columns, Column*, j + 1);
        columns[j] = NULL;

        // Call the Python upstream to determine the strategy where the
        // DataTable should be created.
        PyObject *r = PyObject_CallMethod(freader, "_get_destination", "n", alloc_size);
        targetdir = TOSTRING(r, NULL);  // This will also dec-ref `res`
        if (targetdir == (char*)-1) goto fail;
    } else {
        assert(dt != NULL && ncols == ncols_);
        columns = dt->columns;
        for (int i = 0; i < ncols; i++)
            nstrcols += (types[i] == CT_STRING);
    }

    // Compute number of digits in `ncols` (needed for creating file names).
    if (targetdir) {
        ndigits = 0;
        for (int nc = ncols; nc; nc /= 10) ndigits++;
    }

    // Create individual columns
    for (int i = 0, j = 0; i < ncols_; i++) {
        int8_t type = types[i];
        if (type == CT_DROP) continue;
        if (type > 0) {
            SType stype = colType_to_stype[type];
            columns[j] = realloc_column(columns[j], stype, nrows, j);
            if (columns[j] == NULL) goto fail;
        }
        j++;
    }

    if (dt == NULL) {
        dt = new DataTable(columns);
        if (dt == NULL) goto fail;
    }
    return 1;

  fail:
    if (columns) {
        Column **col = columns;
        while (*col++) delete (*col);
        dtfree(columns);
    }
    return 0;
}



void setFinalNrow(size_t nrows) {
    int i, j;
    for (i = j = 0; i < ncols; i++) {
        int type = types[i];
        if (type == CT_DROP) continue;
        Column *col = dt->columns[j];
        if (type == CT_STRING) {
            StrBuf *sb = (StrBuf*) col->meta;
            assert(sb->numuses == 0);
            void *final_ptr = (void*) sb->buf;
            size_t curr_size = sb->ptr;
            size_t padding = Column::i4s_padding(curr_size);
            size_t offoff = curr_size + padding;
            size_t offs_size = 4 * nrows;
            size_t final_size = offoff + offs_size;
            if (targetdir) {
                snprintf(fname, 1000, "%s/str%0*d", targetdir, ndigits, (int)j);
                truncate(fname, (off_t)final_size);
                munmap(final_ptr, sb->size);
                int fd = open(fname, O_RDWR);
                final_ptr = mmap(NULL, final_size, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
                close(fd);
            } else {
                dtrealloc_g(final_ptr, void, final_size);
            }
            memset(add_ptr(final_ptr, curr_size), 0xFF, padding);
            memcpy(add_ptr(final_ptr, offoff), col->mbuf->get(), offs_size);
            ((int32_t*)add_ptr(final_ptr, offoff))[-1] = -1;
            if (targetdir) {
                snprintf(fname2, 1000, "%s/col%0*d", targetdir, ndigits, (int)j);
                remove(fname2);
                int ret = rename(fname, fname2);
                if (ret == -1) printf("Unable to rename: %d\n", errno);
            } else {
                col->mbuf->release();
            }
            col->mbuf = new MemoryMemBuf(final_ptr, final_size);
            col->nrows = (int64_t) nrows;
            dtrealloc_g(col->meta, VarcharMeta, 1);
            ((VarcharMeta*) col->meta)->offoff = (int64_t) offoff;
        } else if (type > 0) {
            Column *c = realloc_column(col, colType_to_stype[type], nrows, j);
            if (c == NULL) goto fail;
        }
        j++;
    }
    dt->nrows = (int64_t) nrows;
    return;
  fail:
    printf("setFinalNrow() failed!\n");
}


void prepareThreadContext(ThreadLocalFreadParsingContext *ctx)
{
    ctx->strbufs = NULL;
    dtcalloc_g(ctx->strbufs, StrBuf, nstrcols);
    for (int i = 0, j = 0, k = 0, off8 = 0; i < (int)ncols; i++) {
        if (types[i] == CT_DROP) continue;
        if (types[i] == CT_STRING) {
            assert(k < nstrcols);
            dtmalloc_g(ctx->strbufs[k].buf, char, 4096);
            ctx->strbufs[k].size = 4096;
            ctx->strbufs[k].ptr = 0;
            ctx->strbufs[k].idx8 = off8;
            ctx->strbufs[k].idxdt = j;
            k++;
        }
        off8 += (sizes[i] == 8);
        j++;
    }
    return;

  fail:
    printf("prepareThreadContext() failed\n");
    for (int k = 0; k < nstrcols; k++) dtfree(ctx->strbufs[k].buf);
    dtfree(ctx->strbufs);
    *(ctx->stopTeam) = 1;
}


void postprocessBuffer(ThreadLocalFreadParsingContext *ctx)
{
    StrBuf *ctx_strbufs = ctx->strbufs;
    const unsigned char *anchor = (const unsigned char*) ctx->anchor;
    size_t nrows = ctx->nRows;
    lenOff *__restrict__ const lenoffs = (lenOff *__restrict__) ctx->buff8;
    int rowCount8 = (int) ctx->rowSize8 / 8;

    for (int k = 0; k < nstrcols; k++) {
        assert(ctx_strbufs != NULL);

        lenOff *__restrict__ lo = lenoffs + ctx_strbufs[k].idx8;
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
    for (int k = 0; k < nstrcols; k++) {
        int j = ctx_strbufs[k].idxdt;
        StrBuf *sb = (StrBuf*) dt->columns[j]->meta;
        size_t sz = ctx_strbufs[k].ptr;
        size_t ptr = sb->ptr;
        // If we need to write more than the size of the available buffer, the
        // buffer has to grow. Check documentation for `StrBuf.numuses` in
        // `py_fread.h`.
        while (ptr + sz > sb->size) {
            size_t newsize = (ptr + sz) * 2;
            int old = 0;
            // (1) wait until no other process is writing into the buffer
            while (sb->numuses > 0)
                /* wait until .numuses == 0 (all threads finished writing) */;
            // (2) make `numuses` negative, indicating that no other thread may
            // initiate a memcopy operation for now.
            #pragma omp atomic capture
            { old = sb->numuses; sb->numuses -= 1000000; }
            // (3) The only case when `old != 0` is if another thread started
            // memcopy operation in-between statements (1) and (2) above. In
            // that case we restore the previous value of `numuses` and repeat
            // the loop.
            // Otherwise (and it is the most common case) we reallocate the
            // buffer and only then restore the `numuses` variable.
            if (old == 0) {
                if (targetdir) {
                    snprintf(fname, 1000, "%s/str%0*d", targetdir, ndigits, j);
                    truncate(fname, (off_t)newsize);
                    int fd = open(fname, O_RDWR);
                    munmap(sb->buf, sb->size);
                    sb->buf = (char*) mmap(NULL, newsize, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
                    close(fd);
                } else {
                    dtrealloc_g(sb->buf, char, newsize);
                }
                sb->size = newsize;
            }
            #pragma omp atomic update
            sb->numuses += 1000000;
        }
        ctx_strbufs[k].ptr = ptr;
        sb->ptr = ptr + sz;
    }
    return;
  fail:
    printf("orderBuffer() failed");
    *(ctx->stopTeam) = 1;
}


void pushBuffer(ThreadLocalFreadParsingContext *ctx)
{
    StrBuf *__restrict__ ctx_strbufs = ctx->strbufs;
    const void *__restrict__ buff8 = ctx->buff8;
    const void *__restrict__ buff4 = ctx->buff4;
    const void *__restrict__ buff1 = ctx->buff1;
    int nrows = (int) ctx->nRows;
    size_t row0 = ctx->DTi;

    int i = 0;  // index within the `types` and `sizes`
    int j = 0;  // index within `dt->columns`, `buff` and `strbufs`
    int off8 = 0, off4 = 0, off1 = 0;  // offsets within the buffers
    int rowCount8 = (int) ctx->rowSize8 / 8;
    int rowCount4 = (int) ctx->rowSize4 / 4;
    int rowCount1 = (int) ctx->rowSize1;

    int k = 0;
    for (; i < ncols; i++) {
        if (types[i] == CT_DROP) continue;
        Column *col = dt->columns[j];

        if (types[i] == CT_STRING) {
            StrBuf *sb = (StrBuf*) col->meta;
            int idx8 = ctx_strbufs[k].idx8;
            size_t ptr = ctx_strbufs[k].ptr;
            const lenOff *__restrict__ lo =
                (const lenOff*) add_constptr(buff8, idx8 * 8);
            size_t sz = (size_t) abs(lo[(nrows - 1)*rowCount8].off) - 1;

            int done = 0;
            while (!done) {
                int old;
                #pragma omp atomic capture
                old = sb->numuses++;
                if (old >= 0) {
                    memcpy(sb->buf + ptr, ctx_strbufs[k].buf, sz);
                    done = 1;
                }
                #pragma omp atomic update
                sb->numuses--;
            }

            int32_t* dest = ((int32_t*) col->data()) + row0;
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
                uint64_t *dest = ((uint64_t*) col->data()) + row0;
                for (int r = 0; r < nrows; r++) {
                    *dest = *src;
                    src += rowCount8;
                    dest++;
                }
            } else
            if (elemsize == 4) {
                const uint32_t *src = ((const uint32_t*) buff4) + off4;
                uint32_t *dest = ((uint32_t*) col->data()) + row0;
                for (int r = 0; r < nrows; r++) {
                    *dest = *src;
                    src += rowCount4;
                    dest++;
                }
            } else
            if (elemsize == 1) {
                const uint8_t *src = ((const uint8_t*) buff1) + off1;
                uint8_t *dest = ((uint8_t*) col->data()) + row0;
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
        for (int k = 0; k < nstrcols; k++) {
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
    PyObject_CallMethod(flogger, "debug", "O", PyUnicode_FromString(msg));
}
