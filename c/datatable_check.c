#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "datatable.h"
#include "rowindex.h"
#include "types.h"
#include "encodings.h"
#include "utils.h"


//==============================================================================
// Helpers
#define MAX_ERRORS  200

#ifdef __APPLE__
    #include <malloc/malloc.h>  // size_t malloc_size(const void *)
#elif defined _WIN32
    #include <malloc.h>  // size_t _msize(void *)
    #define malloc_size  _msize
#elif defined __linux__
    #include <malloc.h>  // size_t malloc_usable_size(void *) __THROW
    #define malloc_size  malloc_usable_size
#else
    #define malloc_size(p)  0
    #define MALLOC_SIZE_UNAVAILABLE
#endif

/**
 * Return the size of the array `ptr`, or 0 if the platform doesn't allow
 * such functionality.
 */
static size_t array_size(void *ptr, size_t elemsize) {
    #ifdef MALLOC_SIZE_UNAVAILABLE
    return 0;
    #endif
    return ptr == NULL? 0 : malloc_size(ptr) / elemsize;
}

__attribute__ ((format (printf, 3, 0)))
static void push_error(char **out, size_t* len, const char *format, ...) {
    if (*out == NULL) return;
    static char buf[1001];
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buf, 1001, format, args);
    if (ret < 0) return;
    va_end(args);
    size_t n = strlen(buf);
    *out = (char*) realloc(*out, *len + n + 1);
    if (*out == NULL) return;
    memcpy(*out + *len, buf, n + 1);
    *len += n;
}

static char*
repr_utf8(const unsigned char* ptr0, const unsigned char* ptr1) {
    static char buf[101];
    int i = 0;
    for (const unsigned char *ptr = ptr0; ptr < ptr1; ptr++) {
        if (*ptr >= 0x20 && *ptr < 0x7F)
            buf[i++] = (char) *ptr;
        else {
            int8_t d0 = (*ptr) & 0xF;
            int8_t d1 = (*ptr) >> 4;
            buf[i++] = '\\';
            buf[i++] = 'x';
            buf[i++] = d1 <= 9? ('0' + d1) : ('A' + d1 - 10);
            buf[i++] = d0 <= 9? ('0' + d0) : ('A' + d0 - 10);
        }
        if (i >= 95) break;
    }
    buf[i] = '\0';
    return buf;
}



//==============================================================================

/**
 * Check and repair the DataTable.
 *
 * @param dt
 *     The target DataTable.
 *
 * @param errors
 *     Reference to an unallocated `char*` variable that will be allocated and
 *     filled with diagnostic messages containing information about any problems
 *     found. This buffer is allocated up-front regardless of whether there are
 *     any problems in the datatable. It is the responsibility of the caller to
 *     free the memory associated with this string buffer.
 *
 * @return
 *     0 if no problems were found, and the number of errors otherwise.
 */
int dt_verify_integrity(DataTable *dt, char **errors)
{
    if (dt == NULL) return 1;
    int nerrors = 0;
    size_t errlen = 0;
    *errors = (char*) calloc(1, 1);

    RowIndex *ri = dt->rowindex;
    Column **cols = dt->columns;
    Stats **stats = dt->stats;

    #define ERR(...) do {                                                      \
        if (nerrors++ < MAX_ERRORS)                                            \
            push_error(errors, &errlen, __VA_ARGS__);                          \
    } while (0)

    // Check that the number of rows is nonnegative.
    int64_t nrows = dt->nrows;
    if (nrows < 0) {
        ERR("Datatable has negative number of rows: %lld\n", nrows);
        return 1;
    }

    // Check the number of columns; the number of allocated columns should be
    // equal to `ncols + 1` (with extra column being NULL). Sometimes the
    // allocation size can be greater than the required number of columns,
    // because `malloc()` may allocate more than requested.
    size_t n_cols_allocd = array_size(cols, sizeof(Column*));
    int64_t ncols = dt->ncols;
    if (ncols < 0) {
        ERR("Datatable has negative number of columns: %lld\n", ncols);
        return 1;
    }
    if (!cols || !n_cols_allocd) {
        ERR("Columns array is not allocated\n");
        return 1;
    }
    if (ncols + 1 > (int64_t)n_cols_allocd) {
        ERR("Number of columns %lld is larger than the size of the `columns` "
            "array\n", ncols);
        return 1;
    }
    if (cols[ncols] != NULL) {
        // Memory was allocated for `ncols+1` columns, but the last element
        // was not set to NULL.
        // Note that if `cols` array was under-allocated and `malloc_size`
        // not available on this platform, then this might segfault... This is
        // unavoidable since if we skip the check and do `cols[ncols]` later on
        // then we will segfault anyways.
        ERR("Last entry in the `columns` array is not NULL\n");
        return 1;
    }

    if (ri == NULL && stats != NULL) {
        ERR("`stats` array is not NULL when rowindex is NULL");
    } else if (ri != NULL && stats == NULL) {
        ERR("`stats` array is NULL when rowindex is not NULL");
    } else if (ri != NULL && stats != NULL){
        size_t n_stats_allocd = array_size(stats, sizeof(Stats*));

        if ((!stats || !n_stats_allocd) && ncols > 0) {
            ERR("Stats array is not allocated\n");
        }

        if (ncols > (int64_t) n_stats_allocd) {
            ERR("Size of the `stats` array %lld is smaller than the number of "
                "columns %lld (and rowindex is not NULL)\n", (int64_t) n_stats_allocd, ncols);
        }
    }

    // Check that each Column is not NULL
    for (int64_t i = 0; i < ncols; i++) {
        if (cols[i] == NULL) {
            ERR("Column %lld in the datatable is NULL\n", i);
        }
    }
    if (nerrors) return nerrors;

    // Check validity of the RowIndex
    int64_t maxrow = -INT64_MAX;
    int64_t minrow = INT64_MAX;
    if (ri != NULL) {
        RowIndexType ritype = ri->type;
        if (ritype != RI_SLICE && ritype != RI_ARR32 && ritype != RI_ARR64) {
            ERR("Invalid RowIndexType: %d\n", ritype);
            return 1;
        }
        if (ri->length != nrows) {
            ERR("The number of rows in the datatable's rowindex does not "
                "match the number of rows in the datatable itself: %lld vs "
                "%lld\n", ri->length, nrows);
            return 1;
        }
        if (ri->refcount <= 0) {
            ERR("RowIndex has refcount %d which is invalid\n", ri->refcount);
            return 1;
        }
        if (ritype == RI_SLICE) {
            int64_t start = ri->slice.start;
            int64_t step = ri->slice.step;
            int64_t end = start + step * (nrows - 1);
            if (start < 0) {
                ERR("Rowindex's start row is negative: %lld\n", start);
            }
            if (nrows > 1 && end < 0) {  // when nrows==0, end can be negative
                ERR("Rowindex's end row is negative: %lld\n", end);
            }
            if (nrows > 1 && (step < -start/(nrows - 1) ||
                              step > (INT64_MAX - start)/(nrows - 1))) {
                ERR("Integer overflow in slice (%lld..%lld..%lld)\n",
                    start, nrows, step);
            }
            maxrow = step > 0? end : start;
            minrow = step > 0? start : end;
        }
        if (ritype == RI_ARR32) {
            if (nrows > INT32_MAX) {
                ERR("RI_ARR32 rowindex is not allowed for a datatable with "
                    "%lld rows\n", nrows);
            }
            int32_t *ridata = ri->ind32;
            size_t n_allocd = array_size(ridata, sizeof(int32_t));
            if (n_allocd > 0 && n_allocd < (size_t)nrows) {
                ERR("Rowindex array is allocated for %zd elements only, "
                    "while %lld elements were expected\n", n_allocd, nrows);
            }
            for (int32_t i = 0; i < nrows; i++) {
                if (ridata[i] < 0 || ridata[i] > INT32_MAX) {
                    ERR("Rowindex[%d] = %d is invalid\n", i, ridata[i]);
                }
                if (ridata[i] > maxrow) maxrow = ridata[i];
                if (ridata[i] < minrow) minrow = ridata[i];
            }
        }
        if (ritype == RI_ARR64) {
            int64_t *ridata = ri->ind64;
            size_t n_allocd = array_size(ridata, sizeof(int64_t));
            if (n_allocd > 0 && n_allocd < (size_t)nrows) {
                ERR("Rowindex array is allocated for %zd elements only, "
                    "while %lld elements were expected\n", n_allocd, nrows);
            }
            for (int64_t i = 0; i < nrows; i++) {
                if (ridata[i] < 0) {
                    ERR("Rowindex[%lld] = %lld is invalid\n", i, ridata[i]);
                }
                if (ridata[i] > maxrow) maxrow = ridata[i];
                if (ridata[i] < minrow) minrow = ridata[i];
            }
        }
        if (nrows == 0) minrow = maxrow = 0;
        if (ri->min != minrow) {
            ERR("Invalid min.row=%lld in the rowindex, should be %lld\n",
                ri->min, minrow);
        }
        if (ri->max != maxrow) {
            ERR("Invalid max.row=%lld in the rowindex, should be %lld\n",
                ri->max, maxrow);
        }
    }
    if (nerrors) return nerrors;

    // Check each individual column
    for (int64_t i = 0; i < ncols; i++) {
        Column *col = cols[i];
        Stats *stat = ri != NULL ? stats[i] : col->stats;
        SType stype = col->stype;
        MType mtype = cols[i]->mtype;

        if (mtype <= 0 || mtype >= MT_COUNT) {
            ERR("Column %lld has invalid memory type %d\n", i, mtype);
        }
        if (nrows && col->data == NULL) {
            ERR("In column %lld data buffer is not allocated\n", i);
            continue;
        }
        if (mtype == MT_XBUF && !col->pybuf) {
            ERR("Column %lld has mtype MT_XBUF but is missing pybuf ptr\n", i);
            continue;
        }
        if (mtype == MT_TEMP && !col->filename) {
            ERR("Column %lld has mtype MT_TEMP but has no filename\n", i);
            continue;
        }
        if (stype <= ST_VOID || stype >= DT_STYPES_COUNT) {
            ERR("Invalid storage type %d in column %lld\n", stype, i);
            continue;
        }
        if (ri == NULL && col->nrows != nrows) {
            ERR("Column %lld has nrows=%lld, while the datatable has %lld rows\n",
                i, col->nrows, nrows);
            continue;
        }
        if (ri != NULL && col->nrows > 0 && col->nrows <= maxrow) {
            ERR("Column %lld has nrows=%lld, but rowindex references row %lld\n",
                i, col->nrows, maxrow);
            continue;
        }
        if (col->refcount <= 0) {
            ERR("Column's refcount is nonpositive: %d\n", col->refcount);
        }
        size_t metasize = stype_info[stype].metasize;
        if (metasize > 0) {
            if (col->meta == NULL) {
                ERR("Meta information missing for column %lld\n", i);
                continue;
            }
            size_t meta_alloc = array_size(col->meta, 1);
            if (meta_alloc > 0 && meta_alloc < metasize) {
                ERR("Incorrect meta info structure: %zd bytes expected, "
                    "but only %zd allocated\n", metasize, meta_alloc);
                continue;
            }
        }
        int64_t offoff = -1;
        if (stype == ST_STRING_I4_VCHAR || stype == ST_STRING_I8_VCHAR) {
            int is_i4 = (stype == ST_STRING_I4_VCHAR);
            offoff = ((VarcharMeta*) col->meta)->offoff;
            int elemsize = is_i4 ? 4 : 8;
            if (offoff <= 0) {
                ERR("String data section in column %lld has negative length"
                    ": %lld\n", i, offoff);
                continue;
            }
            if (offoff < elemsize) {
                ERR("String data section in column %lld has offset less than "
                    "the element size: %lld\n", i, offoff);
                continue;
            }
            if ((offoff & 7) != 0) {
                ERR("String data section in column %lld has a length which "
                    "is not a multiple of 8: %lld\n", i, offoff);
                continue;
            }
            if (stype == ST_STRING_I4_VCHAR && offoff > INT32_MAX) {
                ERR("String data section in column %lld has length %lld "
                    "which exceeds 32-bit storage limit\n", i, offoff);
                continue;
            }
            size_t datasize = is_i4 ? column_i4s_datasize(col)
                                    : column_i8s_datasize(col);
            size_t exp_padding = is_i4 ? column_i4s_padding(datasize)
                                       : column_i8s_padding(datasize);
            if ((size_t)offoff != datasize + exp_padding) {
                ERR("String column %lld has unexpected offoff=%lld (expected to"
                    "be offoff=%lld)\n", i, offoff, datasize + exp_padding);
                continue;
            }
            for (size_t j = 0; j < exp_padding; j++) {
                unsigned char c = *((unsigned char*)col->data + datasize + j);
                if (c != 0xFF) {
                    ERR("String column %lld is not padded with 0xFF bytes "
                        "correctly: byte %X is %X\n", i, datasize + j, c);
                    break;
                }
            }
        }

        // Check the alloc_size property
        size_t act_allocsize = array_size(col->data, 1);
        size_t elemsize = stype_info[stype].elemsize;
        if (stype == ST_STRING_FCHAR)
            elemsize = (size_t) ((FixcharMeta*) col->meta)->n;
        size_t exp_allocsize = elemsize * (size_t)col->nrows;
        if (offoff > 0)
            exp_allocsize += (size_t)offoff;
        if (col->alloc_size != exp_allocsize) {
            ERR("Column %lld reports incorrect allocation size: %zd vs "
                "expected %zd bytes (actually allocated: %zd bytes)\n",
                i, col->alloc_size, exp_allocsize, act_allocsize);
            continue;
        }
        if (act_allocsize > 0 && act_allocsize < exp_allocsize) {
            ERR("Column %lld has only %zd bytes of data, while %zd bytes "
                "expected\n", i, act_allocsize, exp_allocsize);
            continue;
        }

        // Verify that a boolean column has only values 0, 1 and NA_I1
        if (stype == ST_BOOLEAN_I1) {
            int8_t *data = (int8_t*) col->data;
            for (int64_t j = 0; j < col->nrows; j++) {
                int8_t x = data[j];
                if (!(x == NA_I1 || x == 0 || x == 1)) {
                    ERR("Boolean column %lld has value %d in row %lld\n",
                        i, x, j);
                }
            }
        }

        if (stype == ST_STRING_I4_VCHAR || stype == ST_STRING_I8_VCHAR) {
            #define CASE(T) {                                              \
                T *offsets = (T*) add_ptr(col->data, offoff);              \
                T lastoff = 1;                                             \
                if (offoff && offsets[-1] != -1) {                         \
                    ERR("Number -1 was not found in front of the offsets " \
                        "section\n");                                      \
                }                                                          \
                for (int64_t j = 0; j < col->nrows; j++) {                 \
                    T oj = offsets[j];                                     \
                    if (oj < 0 ? (oj != -lastoff) : (oj < lastoff)) {      \
                        ERR("Invalid offset in column %lld row %lld: "     \
                            "offset = %lld, previous offset = %lld\n",     \
                            i, j, (int64_t)oj, (int64_t)lastoff);          \
                    } else                                                 \
                    if (oj - 1 > offoff) {                                 \
                        ERR("Invalid offset %lld in column %lld row %lld " \
                            "going beyond the end of string data region "  \
                            "(of size %lld)\n", oj, i, j, offoff);         \
                        lastoff = oj + 1;                                  \
                        break;                                             \
                    } else                                                 \
                    if (oj > 0 &&                                          \
                        !is_valid_utf8(cdata + lastoff,                    \
                                       (size_t)(oj - lastoff)))          { \
                        ERR("Invalid utf8 string in column %lld row %lld:" \
                            " '%s'\n", i, j, repr_utf8(cdata + lastoff,    \
                                                       cdata + oj));       \
                    }                                                      \
                    lastoff = oj < 0 ? -oj : oj;                           \
                }                                                          \
                strdata_size = (int64_t) lastoff - 1;                      \
            }
            int64_t strdata_size = 0;
            const uint8_t *cdata = ((const uint8_t*) col->data) - 1;
            if (stype == ST_STRING_I4_VCHAR)
                CASE(int32_t)
            else
                CASE(int64_t)
            #undef CASE

            for (int64_t j = strdata_size; j < offoff; j++) {
                if (((uint8_t*) col->data)[j] != 0xFF) {
                    ERR("String data section in column %lld is not padded "
                        "with '\\xFF's, at offset %X\n", i, j);
                    // Do not report this error more than once
                    break;
                }
            }
        }


        if (stat == NULL) {
            ERR("Stats #%lld is NULL", i);
            continue;
        }
        // If the column's Stats is void, then skip Stats check
        if (stat->is_void()) continue;

        // Check that the column's stored stats are valid
        if (stat->_ref_col != col) {
            ERR("Stats #lld does not refer to its parent column", i);
        }

        /**
         * TODO: Check that all defined stats are appropriate for the column
         *       Additional Stats functionalities are needed to do so
         */
    }

    // The end
    if (nerrors == 1)
        push_error(errors, &errlen, "1 error found.\n");
    if (nerrors >= 2)
        push_error(errors, &errlen, "%d errors found.\n", nerrors);
    return nerrors;
}
