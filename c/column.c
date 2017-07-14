#include <sys/mman.h>
#include <stdio.h>
#include <string.h>    // memcpy, strcmp
#include <stdlib.h>    // atoll
#include <errno.h>
#include <fcntl.h>     // open
#include <unistd.h>    // close
#include "column.h"
#include "myassert.h"
#include "rowmapping.h"
#include "sort.h"
#include "py_utils.h"



/**
 * Simple Column constructor: create a Column with memory type `MT_DATA` and
 * having the provided SType. The column will be preallocated for `nrows` rows,
 * and will have `refcount` = 1. For a variable-width column only the "fixed-
 * -width" part will be allocated.
 *
 * If the column cannot be created (probably due to Out-of-Memory exception),
 * the function will return NULL.
 */
Column* make_data_column(SType stype, size_t nrows)
{
    assert(nrows <= INT64_MAX);
    Column *col = NULL;
    dtmalloc(col, Column, 1);
    col->data = NULL;
    col->meta = NULL;
    col->nrows = (int64_t) nrows;
    col->alloc_size = stype_info[stype].elemsize * nrows;
    col->refcount = 1;
    col->mtype = MT_DATA;
    col->stype = stype;
    dtmalloc(col->meta, void, stype_info[stype].metasize);
    dtmalloc(col->data, void, col->alloc_size);
    return col;
}


Column *make_mmap_column(SType stype, size_t nrows, const char *filename)
{
    assert(nrows <= INT64_MAX);
    size_t alloc_size = stype_info[stype].elemsize * nrows;

    // Create new file of size `alloc_size`.
    FILE *fp = fopen(filename, "w");
    fseek(fp, (off_t)alloc_size - 1, SEEK_SET);
    fputc('\0', fp);
    fclose(fp);

    // Memory-map the file. For some reason I was not able to create memory map
    // without resorting to closing / reopening the file (even with fflush()),
    // it was producing Permission Denied error for some reason.
    int fd = open(filename, O_RDWR);
    void *mmp = mmap(NULL, alloc_size, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
    if (mmp == MAP_FAILED) {
        printf("Memory map failed with errno %d: %s\n", errno, strerror(errno));
        return NULL;
    }
    close(fd);

    Column *col = NULL;
    dtmalloc(col, Column, 1);
    col->data = mmp;
    col->meta = NULL;
    col->nrows = (int64_t) nrows;
    col->stype = stype;
    col->mtype = MT_MMAP;
    col->refcount = 1;
    col->alloc_size = alloc_size;
    dtmalloc(col->meta, void, stype_info[stype].metasize);
    return col;
}



/**
 * Save this column's data buffer into file `filename`. Other information
 * about the column should be stored elsewhere (for example in the _meta.nff
 * file).
 * If a file with the given name already exists, it will be overwritten.
 */
Column* column_save_to_disk(Column *self, const char *filename)
{
    size_t size = self->alloc_size;

    // Open and memory-map the file
    int fd = open(filename, O_RDWR|O_CREAT, 0666);
    if (fd == -1) { printf("Cannot open file %s: %s\n", filename, strerror(errno)); return NULL; }
    int ret = ftruncate(fd, (off_t)size);
    if (ret == -1) { printf("Cannot truncate: %s\n", strerror(errno)); return NULL; }
    void *mmp = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (mmp == MAP_FAILED) { printf("Cannot memory-map file %s: %s", filename, strerror(errno)); return NULL; }
    close(fd);

    // Copy the data buffer into the file
    memcpy(mmp, self->data, size);
    munmap(mmp, size);
    return self;
}



/**
 * Restore a Column previously saved via `column_save_to_disk()`. The column's
 * data buffer is taken from the file `filename`; and the column is assumed to
 * have type `stype`, number of rows `nrows`, and its meta information stored
 * as a string `metastr`.
 * This function will not check data validity (i.e. that the buffer contains
 * valid values, and that the extra parameters match the buffer's contents).
 */
Column* column_load_from_disk(const char *filename, SType stype, int64_t nrows,
                              const char *metastr)
{
    // Deserialize the meta information, if needed
    void *meta = NULL;
    if (stype == ST_STRING_I4_VCHAR || stype == ST_STRING_I8_VCHAR) {
        if (strncmp(metastr, "offoff=", 7) != 0) return NULL;
        int64_t offoff = (int64_t) atoll(metastr + 7);
        dtmalloc(meta, VarcharMeta, 1);
        ((VarcharMeta*) meta)->offoff = offoff;
    }

    // Open and memory-map the file
    int fd = open(filename, O_RDONLY);
    if (fd == -1) { printf("Cannot open file %s: %s\n", filename, strerror(errno)); return NULL; }
    struct stat stat_buf;
    int ret = fstat(fd, &stat_buf);
    if (ret == -1) { printf("Cannot obtain file's size: %s\n", strerror(errno)); return NULL; }
    size_t filesize = (size_t) stat_buf.st_size;
    void *mmp = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mmp == MAP_FAILED) { printf("Cannot memory-map file %s: %s", filename, strerror(errno)); return NULL; }
    close(fd);

    // Create the column object
    Column *col = NULL;
    dtmalloc(col, Column, 1);
    col->data = mmp;
    col->meta = meta;
    col->nrows = nrows;
    col->alloc_size = filesize;
    col->refcount = 1;
    col->mtype = MT_MMAP;
    col->stype = stype;

    return col;
}


/**
 * Make a "deep" copy of the column. The column created with this method will
 * have memory-type MT_DATA and refcount of 1.
 * If a shallow copy is needed, then simply copy the column's reference and
 * call `column_incref()`.
 */
Column* column_copy(Column *self)
{
    size_t alloc_size = self->alloc_size;
    size_t meta_size = stype_info[self->stype].metasize;
    Column *col = NULL;
    dtmalloc(col, Column, 1);
    col->data = NULL;
    col->meta = NULL;
    col->nrows = self->nrows;
    col->stype = self->stype;
    col->mtype = MT_DATA;
    col->refcount = 1;
    col->alloc_size = alloc_size;
    if (alloc_size) {
        dtmalloc(col->data, void, alloc_size);
        memcpy(col->data, self->data, alloc_size);
    }
    if (meta_size) {
        dtmalloc(col->meta, void, meta_size);
        memcpy(col->meta, self->meta, meta_size);
    }
    return col;
}



/**
 * Extract data from this column at rows specified in the provided `rowmapping`.
 * A new Column with type `MT_DATA` will be created and returned. The
 * `rowmapping` parameter can also be NULL, in which case a shallow copy
 * is returned (if a "deep" copy is needed, then use `column_copy()`).
 */
Column* column_extract(Column *self, RowMapping *rowmapping)
{
    // If `rowmapping` is not provided, then return a shallow "copy".
    if (rowmapping == NULL) {
        self->refcount++;
        return self;
    }

    SType stype = self->stype;
    size_t nrows = (size_t) rowmapping->length;
    size_t elemsize = (stype == ST_STRING_FCHAR)
                        ? (size_t) ((FixcharMeta*) self->meta)->n
                        : stype_info[stype].elemsize;

    // Create the new Column object.
    Column *res = make_data_column(stype, 0);
    res->nrows = (int64_t) nrows;

    // "Slice" rowmapping with step = 1 is a simple subsection of the column
    if (rowmapping->type == RM_SLICE && rowmapping->slice.step == 1) {
        size_t start = (size_t) rowmapping->slice.start;
        switch (stype) {
            #define CASE_IX_VCHAR(etype, abs) {                                \
                size_t offoff = (size_t)((VarcharMeta*) self->meta)->offoff;   \
                etype *offs = (etype*)(self->data + offoff) + start;           \
                etype off0 = start? abs(*(offs - 1)) - 1 : 0;                  \
                etype off1 = start + nrows? abs(*(offs + nrows - 1)) - 1 : 0;  \
                size_t datasize = (size_t)(off1 - off0);                       \
                size_t padding = (8 - (datasize & 7)) & 7;                     \
                size_t offssize = nrows * elemsize;                            \
                offoff = datasize + padding;                                   \
                res->alloc_size = datasize + padding + offssize;               \
                res->data = TRY(malloc(res->alloc_size));                      \
                ((VarcharMeta*) res->meta)->offoff = (int64_t)offoff;          \
                memcpy(res->data, self->data + (size_t)off0, datasize);        \
                memset(res->data + datasize, 0xFF, padding);                   \
                etype *resoffs = (etype*)(res->data + offoff);                 \
                for (size_t i = 0; i < nrows; i++) {                           \
                    resoffs[i] = offs[i] > 0? offs[i] - off0                   \
                                            : offs[i] + off0;                  \
                }                                                              \
            } break;
            case ST_STRING_I4_VCHAR: CASE_IX_VCHAR(int32_t, abs)
            case ST_STRING_I8_VCHAR: CASE_IX_VCHAR(int64_t, llabs)
            #undef CASE_IX_VCHAR

            case ST_STRING_U1_ENUM:
            case ST_STRING_U2_ENUM:
            case ST_STRING_U4_ENUM:
                assert(0);  // not implemented yet
                break;

            default: {
                assert(!stype_info[stype].varwidth);
                size_t alloc_size = nrows * elemsize;
                size_t offset = start * elemsize;
                res->data = TRY(clone(self->data + offset, alloc_size));
                res->alloc_size = alloc_size;
            } break;
        }
        return res;
    }

    // In all other cases we need to iterate through the rowmapping and fetch
    // the required elements manually.
    switch (stype) {
        #define JINIT_SLICE                                                    \
            int64_t start = rowmapping->slice.start;                           \
            int64_t step = rowmapping->slice.step;                             \
            int64_t j = start - step;
        #define JITER_SLICE                                                    \
            j += step;
        #define JINIT_ARR(bits)                                                \
            intXX(bits) *rowindices = rowmapping->ind ## bits;
        #define JITER_ARR(bits)                                                \
            intXX(bits) j = rowindices[i];

        #define CASE_IX_VCHAR_SUB(ctype, abs, JINIT, JITER) {                  \
            size_t offoff = (size_t)((VarcharMeta*) self->meta)->offoff;       \
            ctype *offs = (ctype*)(self->data + offoff);                       \
            size_t datasize = 0;                                               \
            {   JINIT                                                          \
                for (size_t i = 0; i < nrows; i++) {                           \
                    JITER                                                      \
                    if (offs[j] > 0) {                                         \
                        ctype prevoff = j? abs(offs[j - 1]) : 1;               \
                        datasize += (size_t)(offs[j] - prevoff);               \
                    }                                                          \
                }                                                              \
            }                                                                  \
            size_t padding = (8 - (datasize & 7)) & 7;                         \
            size_t offssize = nrows * elemsize;                                \
            offoff = datasize + padding;                                       \
            res->alloc_size = offoff + offssize;                               \
            res->data = (char*) TRY(malloc(res->alloc_size));                  \
            ((VarcharMeta*) res->meta)->offoff = (int64_t) offoff;             \
            {   JINIT                                                          \
                ctype lastoff = 1;                                             \
                char *dest = res->data;                                        \
                ctype *resoffs = (ctype*)(res->data + offoff);                 \
                for (size_t i = 0; i < nrows; i++) {                           \
                    JITER                                                      \
                    if (offs[j] > 0) {                                         \
                        ctype prevoff = j? abs(offs[j - 1]) : 1;               \
                        size_t len = (size_t)(offs[j] - prevoff);              \
                        if (len) {                                             \
                            memcpy(dest, self->data + prevoff - 1, len);       \
                            dest += len;                                       \
                            lastoff += len;                                    \
                        }                                                      \
                        resoffs[i] = lastoff;                                  \
                    } else                                                     \
                        resoffs[i] = -lastoff;                                 \
                }                                                              \
                memset(dest, 0xFF, padding);                                   \
            }                                                                  \
        }
        #define CASE_IX_VCHAR(ctype, abs)                                      \
            if (rowmapping->type == RM_SLICE)                                  \
                CASE_IX_VCHAR_SUB(ctype, abs, JINIT_SLICE, JITER_SLICE)        \
            else if (rowmapping->type == RM_ARR32)                             \
                CASE_IX_VCHAR_SUB(ctype, abs, JINIT_ARR(32), JITER_ARR(32))    \
            else if (rowmapping->type == RM_ARR64)                             \
                CASE_IX_VCHAR_SUB(ctype, abs, JINIT_ARR(64), JITER_ARR(64))    \
            break;

        case ST_STRING_I4_VCHAR: CASE_IX_VCHAR(int32_t, abs)
        case ST_STRING_I8_VCHAR: CASE_IX_VCHAR(int64_t, llabs)

        #undef CASE_IX_VCHAR_SUB
        #undef CASE_IX_VCHAR
        #undef JINIT_SLICE
        #undef JINIT_ARRAY
        #undef JITER_SLICE
        #undef JITER_ARRAY

        case ST_STRING_U1_ENUM:
        case ST_STRING_U2_ENUM:
        case ST_STRING_U4_ENUM:
            assert(0);  // not implemented yet
            break;

        default: {
            assert(!stype_info[stype].varwidth);
            size_t alloc_size = nrows * elemsize;
            res->data = TRY(malloc(alloc_size));
            res->alloc_size = alloc_size;
            char *dest = res->data;
            if (rowmapping->type == RM_SLICE) {
                size_t stepsize = (size_t) rowmapping->slice.step * elemsize;
                char *src = self->data + (size_t) rowmapping->slice.start * elemsize;
                for (size_t i = 0; i < nrows; i++) {
                    memcpy(dest, src, elemsize);
                    dest += elemsize;
                    src += stepsize;
                }
            } else
            if (rowmapping->type == RM_ARR32) {
                int32_t *rowindices = rowmapping->ind32;
                for (size_t i = 0; i < nrows; i++) {
                    size_t j = (size_t) rowindices[i];
                    memcpy(dest, self->data + j*elemsize, elemsize);
                    dest += elemsize;
                }
            } else
            if (rowmapping->type == RM_ARR32) {
                int64_t *rowindices = rowmapping->ind64;
                for (size_t i = 0; i < nrows; i++) {
                    size_t j = (size_t) rowindices[i];
                    memcpy(dest, self->data + j*elemsize, elemsize);
                    dest += elemsize;
                }
            }
        } break;
    }
    return res;

  fail:
    free(res->meta);
    free(res->data);
    free(res);
    return NULL;
}



Column* column_cast(Column *self, SType stype)
{
    // Not implemented :(
    (void)self;
    (void)stype;
    return NULL;
}



RowMapping* column_sort(Column *col, int64_t nrows)
{
    assert(col->nrows == nrows);
    if (col->stype == ST_INTEGER_I4) {
        int32_t *ordering = NULL;
        sort_i4(col->data, (int32_t)nrows, &ordering);
        return rowmapping_from_i32_array(ordering, (int32_t)nrows);
    }
    return rowmapping_from_slice(0, nrows, 1);
}




/**
 * Increase reference count on column `self`. This function should be called
 * when a new long-term copy to the column is created (for example if the column
 * is referenced from several data tables). For convenience, this function
 * returns the column object passed.
 * Here `self` can also be NULL, in which case this function does nothing.
 */
Column* column_incref(Column *self)
{
    if (self == NULL) return NULL;
    self->refcount++;
    return self;
}



/**
 * Decrease reference count on column `self`. Call this function when disposing
 * of a previously held pointer to a column. If the internal reference counter
 * reaches 0, the column's data will be garbage-collected.
 * Here `self` can also be NULL, in which case this function does nothing.
 */
void column_decref(Column *self)
{
    if (self == NULL) return;
    self->refcount--;
    if (self->refcount <= 0) {
        if (self->mtype == MT_DATA) {
            dtfree(self->data);
        }
        if (self->mtype == MT_MMAP) {
            munmap(self->data, self->alloc_size);
        }
        dtfree(self->meta);
        dtfree(self);
    }
}



/**
 * Compute the amount of padding between the data and offset section for an
 * ST_STRING_I4_VCHAR column. The formula ensures that datasize + padding are
 * always 8-byte aligned, and that the amount of padding is at least 4 bytes.
 */
size_t column_i4s_padding(size_t datasize) {
    return ((8 - ((datasize + 4) & 7)) & 7) + 4;
}
