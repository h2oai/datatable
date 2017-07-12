#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "datatable.h"
#include "rowmapping.h"
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
    int ret = vsnprintf(buf, 1000, format, args);
    if (ret < 0) return;
    size_t n = (size_t) ret;
    va_end(args);
    *out = (char*) realloc(*out, *len + n);
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
 *     Reference to a `char*` variable that will be filled with diagnostic
 *     messages containing information about any problems found. When this
 *     function exits, it becomes the responsibility of the caller to free the
 *     memory associated with this string buffer.
 *
 * @param fix
 *     If true, then the function will make an attempt to fix some of the
 *     errors that it discovers.
 *
 * @return
 *     OR-ed constants DTCK_* defined in "datatable.h". In particular, the
 *     return value is 0 if there were no problems with the datatable. If
 *     errors were found and then fixed, then the return value is 3; whereas
 *     if errors were found but not all of them fixed, then the return value
 *     is 1.
 */
int dt_verify_integrity(DataTable *dt, char **errors, _Bool fix)
{
    if (dt == NULL) return DTCK_RUNTIME_ERROR;
    int found_errors = 0;
    int fixed_errors = 0;
    size_t errlen = 0;
    *errors = (char*) calloc(1, 1);

    RowMapping *rm = dt->rowmapping;
    Column **cols = dt->columns;

    #define ERR(...) do {                                                      \
        if (found_errors++ < MAX_ERRORS)                                       \
            push_error(errors, &errlen, __VA_ARGS__);                          \
    } while (0)


    // Check that the number of rows is nonnegative.
    int64_t nrows = dt->nrows;
    if (nrows < 0) {
        ERR("Number of rows is negative: %lld\n", nrows);
        if (!fix) return DTCK_ERRORS_FOUND;
        dt->nrows = nrows = 0;
        fixed_errors++;
    }

    // Check the number of columns; the number of allocated columns should be
    // equal to `ncols + 1` (with extra column being NULL). Sometimes the
    // allocation size can be greater than the required number of columns,
    // because `malloc()` may allocate more than requested.
    size_t n_cols_allocd = array_size(cols, sizeof(Column*));
    int64_t ncols = dt->ncols;
    if (ncols < 0) {
        ERR("Number of columns is negative: %lld\n", ncols);
        if (!fix) return DTCK_ERRORS_FOUND;
        dt->ncols = ncols = 0;
        dt->columns = (Column**) realloc(cols, sizeof(Column*));
        dt->columns[0] = NULL;
        fixed_errors++;
    }
    if (ncols + 1 > (int64_t)n_cols_allocd && n_cols_allocd >= 1) {
        ERR("Number of columns %lld is larger than the size of the `columns` "
            "array\n", ncols);
        if (!fix) return DTCK_ERRORS_FOUND;
        dt->ncols = ncols = (int64_t) n_cols_allocd - 1;
        dt->columns[ncols] = NULL;
        fixed_errors++;
    }
    if (cols[ncols] != NULL) {
        // Memory was allocated for `ncols+1` columns, but the last element
        // was not set to NULL. Do not attempt to free that element, since it
        // may just be a leftover memory value.
        // Note that if `cols` was array was under-allocated and `malloc_size`
        // not available on this platform, then this might segfault... This is
        // unavoidable since if we skip the check and do `cols[ncols]` later on
        // then we will segfault anyways.
        ERR("Last entry in the `columns` array is not NULL\n");
        if (!fix) return DTCK_ERRORS_FOUND;
        cols[ncols] = NULL;
        fixed_errors++;
    }


    // Check that each Column is not NULL
    {
        int64_t i = 0, j = 0;
        for (; i < ncols; i++) {
            if (cols[i] == NULL) {
                ERR("Column %lld in the datatable is NULL\n", i);
                if (fix) fixed_errors++;  // fixed later in `if (fix && i > j)`
            } else {
                int mtype = cols[i]->mtype;
                if (mtype != MT_DATA && mtype != MT_MMAP) {
                    ERR("Column %lld has unknown memory type %d\n", i, mtype);
                    if (fix) {
                        cols[i]->mtype = MT_DATA;
                        fixed_errors++;
                    }
                }
                if (fix && i > j) {
                    cols[j] = cols[i];
                    cols[i] = NULL;
                }
                j++;
            }
        }
        if (fixed_errors < found_errors) return DTCK_ERRORS_FOUND;
        if (fix && i > j) {
            dt->ncols = ncols = j;
            dt->columns = (Column**) realloc(dt->columns, (size_t) ncols + 1);
        }
    }


    // Check validity of the RowMapping
    int64_t maxrow = -1;
    if (rm != NULL) {
        RowMappingType rmtype = rm->type;
        if (rmtype != RM_SLICE && rmtype != RM_ARR32 && rmtype != RM_ARR64) {
            ERR("Invalid RowMappingType: %d\n", rmtype);
            return DTCK_ERRORS_FOUND;
        }
        if (rm->length != nrows) {
            ERR("The number of rows in the datatable's rowmapping does not "
                "match the number of rows in the datatable itself: %lld vs "
                "%lld\n", rm->length, nrows);
            return DTCK_ERRORS_FOUND;
        }
        if (rmtype == RM_SLICE) {
            int64_t start = rm->slice.start;
            int64_t step = rm->slice.step;
            int64_t end = start + step * (nrows - 1);
            if (start < 0) {
                ERR("Rowmapping's start row is negative: %lld", start);
                return DTCK_ERRORS_FOUND;
            }
            if (end < 0) {
                ERR("Rowmapping's end row is negative: %lld", end);
                return DTCK_ERRORS_FOUND;
            }
            if (nrows > 1 && (step < -start/(nrows - 1) ||
                              step > (INT64_MAX - start)/(nrows - 1))) {
                ERR("Integer overflow in slice (%lld..%lld..%lld)\n",
                    start, nrows, step);
                return DTCK_ERRORS_FOUND;
            }
            maxrow = step > 0? end : start;
        }
        if (rmtype == RM_ARR32) {
            if (nrows > INT32_MAX) {
                ERR("RM_ARR32 rowmapping is not allowed for a datatable with "
                    "%lld rows", nrows);
                return DTCK_ERRORS_FOUND;
            }
            int32_t *rmdata = rm->ind32;
            size_t n_allocd = array_size(rmdata, sizeof(int32_t));
            if (n_allocd > 0 && n_allocd < (size_t)nrows) {
                ERR("Rowmapping array is allocated for %zd elements only, "
                    "while %lld elements were expected\n", n_allocd, nrows);
                return DTCK_ERRORS_FOUND;
            }
            for (int32_t i = 0; i < nrows; i++) {
                if (rmdata[i] < 0 || rmdata[i] > INT32_MAX) {
                    ERR("Rowmapping[%d] = %d is invalid\n", i, rmdata[i]);
                }
                if (rmdata[i] > maxrow) maxrow = rmdata[i];
            }
        }
        if (rmtype == RM_ARR64) {
            int64_t *rmdata = rm->ind64;
            size_t n_allocd = array_size(rmdata, sizeof(int64_t));
            if (n_allocd > 0 && n_allocd < (size_t)nrows) {
                ERR("Rowmapping array is allocated for %zd elements only, "
                    "while %lld elements were expected\n", n_allocd, nrows);
                return DTCK_ERRORS_FOUND;
            }
            for (int64_t i = 0; i < nrows; i++) {
                if (rmdata[i] < 0) {
                    ERR("Rowmapping[%lld] = %lld is invalid\n", i, rmdata[i]);
                }
                if (rmdata[i] > maxrow) maxrow = rmdata[i];
            }
        }
    }
    if (found_errors > fixed_errors) return DTCK_ERRORS_FOUND;


    // Check each individual column
    for (int64_t i = 0; i < ncols; i++) {
        Column *col = cols[i];
        SType stype = col->stype;

        if (stype <= ST_VOID || stype >= DT_STYPES_COUNT) {
            ERR("Invalid storage type %d in column %lld\n", stype, i);
            continue;
        }
        if (rm == NULL && col->nrows != nrows) {
            ERR("Column %lld has nrows=%lld, while the datatable has %lld rows\n",
                i, col->nrows, nrows);
            continue;
        }
        if (rm != NULL && col->nrows <= maxrow) {
            ERR("Column %lld has nrows=%lld, but rowmapping references row %lld\n",
                i, col->nrows, maxrow);
            continue;
        }
        if (col->refcount <= 0) {
            ERR("Column's refcount is nonpositive: %d\n", col->refcount);
            if (fix) {
                col->refcount = 1;
                fixed_errors++;
            } else continue;
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
            offoff = ((VarcharMeta*) col->meta)->offoff;
            int elemsize = stype == ST_STRING_I4_VCHAR? 4 : 8;
            if (offoff <= 0) {
                ERR("String data section in column %lld has negative length"
                    ": %lld\n", i, offoff);
                continue;
            }
            if ((offoff & (elemsize - 1)) != 0) {
                ERR("String data section in column %lld has a length which "
                    "is not a multiple of %d: %lld\n", i, elemsize, offoff);
                // This might be fixable... unclear
                continue;
            }
            if (stype == ST_STRING_I4_VCHAR && offoff > INT32_MAX) {
                ERR("String data section in column %lld has length %lld "
                    "which exceeds 32-bit storage limit\n", i, offoff);
                // Might also be fixable
                continue;
            }
        }

        size_t act_allocsize = array_size(col->data, 1);
        size_t elemsize = stype_info[stype].elemsize;
        if (stype == ST_STRING_FCHAR)
            elemsize = (size_t) ((FixcharMeta*) col->meta)->n;
        size_t exp_allocsize = elemsize * (size_t)nrows;
        if (offoff > 0)
            exp_allocsize += (size_t)offoff;
        if (col->alloc_size != exp_allocsize) {
            ERR("Column %lld reports incorrect allocation size: %zd vs "
                "expected %zd bytes (actually allocated: %zd bytes)\n",
                i, col->alloc_size, exp_allocsize, act_allocsize);
            if (fix) {
                col->alloc_size = exp_allocsize;
                fixed_errors++;
            } else continue;
        }
        if (act_allocsize > 0 && act_allocsize < exp_allocsize) {
            ERR("Column %lld has only %zd bytes of data, while %zd bytes "
                "expected\n", i, act_allocsize, exp_allocsize);
            continue;
        }

        // Verify that a boolean column has only values 0, 1 and NA_I1
        if (stype == ST_BOOLEAN_I1) {
            int8_t *data = (int8_t*) col->data;
            for (int64_t j = 0; j < nrows; j++) {
                int8_t x = data[j];
                if (!(x == NA_I1 || x == 0 || x == 1)) {
                    ERR("Boolean column %lld has value %d in row %lld\n",
                        i, x, j);
                    if (fix) {
                        data[j] = NA_I1;
                        fixed_errors++;
                    }
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
                for (int64_t j = 0; j < nrows; j++) {                      \
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
                        "with '\\xFF's", i);
                    if (fix) {
                        ((unsigned char*) col->data)[j] = 0xFF;
                        fixed_errors++;
                    } else {
                        // Do not report this error more than once
                        break;
                    }
                }
            }
        }
    }


    // Check rollups...


    // The end
    if (found_errors > 0) {
        push_error(errors, &errlen, "%d error%s found, and %d fixed\n",
                   found_errors, (found_errors == 1? " was" : "s were"),
                   fixed_errors);
        return DTCK_ERRORS_FOUND |
               (found_errors == fixed_errors? DTCK_ERRORS_FIXED : 0);
    } else
        return 0;
}
