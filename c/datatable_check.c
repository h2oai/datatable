#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "datatable.h"
#include "rowmapping.h"
#include "types.h"


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
    *out = realloc(*out, *len + n);
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

static _Bool
is_valid_utf8(const unsigned char* ptr0, const unsigned char* ptr1) {
    const unsigned char *ptr = ptr0;
    while (ptr < ptr1) {
        if (*ptr < 0x80) {
            // 0xxxxxxx
            ptr++;
        } else if ((*ptr & 0xE0) == 0xC0) {
            // 110xxxxx 10xxxxxx
            if ((ptr[1] & 0xC0) != 0x80 || (ptr[0] & 0xFE) == 0xC0)
                return 0;
            ptr += 2;
        } else if ((*ptr & 0xF0) == 0xE0) {
            // 1110xxxx 10xxxxxx 10xxxxxx
            if ((ptr[1] & 0xC0) != 0x80 || (ptr[2] & 0xC0) != 0x80 ||
                (ptr[0] == 0xE0 && (ptr[1] & 0xE0) == 0x80) ||  // overlong
                (ptr[0] == 0xED && (ptr[1] & 0xE0) == 0xA0) ||  // surrogate
                (ptr[0] == 0xEF && ptr[1] == 0xBF && (ptr[2] & 0xFE) == 0xBE))
                return 0;
            ptr += 3;
        } else if ((*ptr & 0xF8) == 0xF0) {
            // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            if ((ptr[1] & 0xC0) != 0x80 ||
                (ptr[2] & 0xC0) != 0x80 ||
                (ptr[3] & 0xC0) != 0x80 ||
                (ptr[0] == 0xF0 && (ptr[1] & 0xF0) == 0x80) ||  // overlong
                (ptr[0] == 0xF4 && ptr[1] > 0x8F) ||  // unmapped
                ptr[0] > 0xF4)
                return 0;
            ptr += 4;
        } else
            return 0;
    }
    return (ptr == ptr1);
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
    *errors = calloc(1, 1);

    RowMapping *rm = dt->rowmapping;
    DataTable *src = dt->source;
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
    // because `malloc()` sometimes allocates more than requested.
    size_t n_cols_allocd = array_size(cols, sizeof(Column*));
    int64_t ncols = dt->ncols;
    if (ncols < 0) {
        ERR("Number of columns is negative: %lld\n", ncols);
        if (!fix) return DTCK_ERRORS_FOUND;
        dt->ncols = ncols = 0;
        dt->columns = realloc(cols, sizeof(Column*));
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


    // Check that each Column is not NULL, and compute the number of "view"
    // columns.
    int64_t n_view_columns = 0;
    {
        int64_t i = 0, j = 0;
        for (; i < ncols; i++) {
            if (cols[i] == NULL) {
                ERR("Column %lld in the datatable is NULL\n", i);
                if (fix) fixed_errors++;  // fixed later in `if (fix && i > j)`
            } else {
                int mtype = cols[i]->mtype;
                n_view_columns += (mtype == MT_VIEW);
                if (mtype < 1 || mtype > 3) {
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
            dt->columns = realloc(dt->columns, (size_t) ncols + 1);
        }
    }


    // Check that "view" datatable is in a consistent state. There are three
    // properties that determine whether a table is a view: (1) `rowmapping` is
    // not NULL, (2) `source` is not NULL, and (3) number of "view" columns is
    // non-zero.
    int p1 = (rm == NULL);
    int p2 = (src == NULL);
    int p3 = (n_view_columns == 0);
    if (p1 != p2 || p1 != p3) {
        ERR("Invalid \"view\" datatable: %s, %s and %s\n",
            p1? "rowmapping is NULL" : "rowmapping is not NULL",
            p2? "source is NULL" : "source is not NULL",
            p3? "there are no view columns" :
                n_view_columns == 1? "there is 1 view column" :
                                     "there are %d view columns",
            n_view_columns);
        // If there are no view columns, then `rowmapping` and `source` can be
        // simply thrown away; otherwise there is no easy fix...
        if (fix && p3) {
            dt->rowmapping = NULL;
            dt->source = NULL;
            fixed_errors++;
        } else return DTCK_ERRORS_FOUND;
    }
    if (src != NULL && src->source != NULL) {
        ERR("View datatable references another datatable which is also a view");
        // This should be possible to fix; just hard...
        return DTCK_ERRORS_FOUND;
    }


    // Check validity of the RowMapping
    if (rm != NULL) {
        RowMappingType rmtype = rm->type;
        if (rmtype < 1 || rmtype > 3) {
            ERR("Invalid RowMappingType: %d\n", rmtype);
            return DTCK_ERRORS_FOUND;
        }
        int64_t src_nrows = src->nrows;
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
            if (start < 0 || start >= src_nrows) {
                ERR("Datatable's rowmapping references invalid row %lld in the "
                    "source datatable (with %lld rows)", start, src_nrows);
                return DTCK_ERRORS_FOUND;
            }
            if (end < 0 || end >= src_nrows) {
                ERR("Datatable's rowmapping references invalid row %lld in the "
                    "source datatable (with %lld rows)", end, src_nrows);
                return DTCK_ERRORS_FOUND;
            }
            if (nrows > 1 && (step < -start/(nrows - 1) ||
                              step > (INT64_MAX - start)/(nrows - 1))) {
                ERR("Integer overflow in slice (%lld..%lld..%lld)\n",
                    start, nrows, step);
                return DTCK_ERRORS_FOUND;
            }
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
            int32_t max = nrows < INT32_MAX? (int32_t) nrows - 1 : INT32_MAX;
            for (int32_t i = 0; i < nrows; i++) {
                if (rmdata[i] < 0 || rmdata[i] > max) {
                    ERR("Rowmapping[%d] = %d is invalid\n", i, rmdata[i]);
                }
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
                if (rmdata[i] < 0 || rmdata[i] >= src_nrows) {
                    ERR("Rowmapping[%lld] = %lld is invalid\n", i, rmdata[i]);
                }
            }
        }
    }
    if (found_errors > fixed_errors) return DTCK_ERRORS_FOUND;


    // Check each individual column
    _Bool cleanup_required = 0;
    for (int64_t i = 0; i < ncols; i++) {
        Column *col = cols[i];
        SType stype = col->stype;

        if (col->mtype == MT_VIEW) {
            size_t srcindex = ((ViewColumn*)col)->srcindex;
            if (srcindex >= (size_t)src->ncols) {
                ERR("Column %lld references non-existing column %zd in the "
                    "source datatable\n", i, srcindex);
                if (fix) {
                    cols[i] = NULL;
                    cleanup_required = 1;
                    fixed_errors++;
                }
                continue;
            }
            SType src_stype = src->columns[srcindex]->stype;
            if (stype != src_stype) {
                ERR("Inconsistent stype in view column %lld: stype = %d, "
                    "src.stype = %d\n", i, stype, src_stype);
                if (fix) {
                    col->stype = src_stype;
                    fixed_errors++;
                }
            }
        }
        else {
            if (stype <= ST_VOID || stype >= DT_STYPES_COUNT) {
                ERR("Invalid storage type %d in column %lld\n", stype, i);
                continue;
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
                if (offoff < 0) {
                    ERR("String data section in column %lld has negative length"
                        ": %lld\n", i, offoff);
                    continue;
                }
                if ((offoff & 7) != 0) {
                    ERR("String data section in column %lld has a length which "
                        "is not a multiple of 8: %lld", i, offoff);
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
                    T *offsets = (T*)(col->data + offoff);                     \
                    T lastoff = 1;                                             \
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
                            !is_valid_utf8(cdata + lastoff-1, cdata + oj-1)) { \
                            ERR("Invalid utf8 string in column %lld row %lld:" \
                                " '%s'\n", i, j, repr_utf8(cdata + lastoff,    \
                                                           cdata + oj));       \
                        }                                                      \
                        lastoff = oj < 0 ? -oj : oj;                           \
                    }                                                          \
                    strdata_size = (int64_t) lastoff - 1;                      \
                }
                int64_t strdata_size = 0;
                const unsigned char *cdata = (const unsigned char*) col->data;
                if (stype == ST_STRING_I4_VCHAR)
                    CASE(int32_t)
                else
                    CASE(int64_t)
                #undef CASE

                for (int64_t j = strdata_size; j < offoff; j++) {
                    if (((unsigned char*) col->data)[j] != 0xFF) {
                        ERR("String data section in column %lld is not padded "
                            "with '\\xFF's", i);
                        if (fix) {
                            ((unsigned char*) col->data)[j] = 0xFF;
                            fixed_errors++;
                        }
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
