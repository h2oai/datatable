#include <sys/mman.h>
#include <stdio.h>
#include <string.h>    // memcpy, strcmp
#include <stdlib.h>    // atoll
#include <errno.h>
#include <fcntl.h>     // open
#include <unistd.h>    // close
#include "column.h"
#include "myassert.h"
#include "rowindex.h"
#include "sort.h"
#include "py_utils.h"

extern void free_xbuf_column(Column *col);


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
    if (mmp == MAP_FAILED) dterre0("Memory-map failed.");
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
    if (fd == -1) dterre("Cannot open file %s", filename);
    int ret = ftruncate(fd, (off_t)size);
    if (ret == -1) dterre("Cannot truncate file %s", filename);
    void *mmp = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (mmp == MAP_FAILED) dterre0("Memory-map failed.");
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
    if (fd == -1) dterre("Cannot open file %s", filename);
    struct stat stat_buf;
    int ret = fstat(fd, &stat_buf);
    if (ret == -1) dterre0("Cannot obtain file's size");
    size_t filesize = (size_t) stat_buf.st_size;
    void *mmp = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mmp == MAP_FAILED) dterre0("Memory-map failed");
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
 * Construct a column from the externally provided buffer.
 */
Column* column_from_buffer(SType stype, int64_t nrows, void* pybuffer,
                           void* buf, size_t alloc_size)
{
    Column *col = NULL;
    dtmalloc(col, Column, 1);
    col->data = buf;
    col->meta = NULL;
    col->nrows = nrows;
    col->alloc_size = alloc_size;
    col->pybuf = pybuffer;
    col->refcount = 1;
    col->mtype = MT_XBUF;
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
 * Extract data from this column at rows specified in the provided `rowindex`.
 * A new Column with type `MT_DATA` will be created and returned. The
 * `rowindex` parameter can also be NULL, in which case a shallow copy
 * is returned (if a "deep" copy is needed, then use `column_copy()`).
 */
Column* column_extract(Column *self, RowIndex *rowindex)
{
    // If `rowindex` is not provided, then return a shallow "copy".
    if (rowindex == NULL) {
        self->refcount++;
        return self;
    }

    SType stype = self->stype;
    size_t nrows = (size_t) rowindex->length;
    size_t elemsize = (stype == ST_STRING_FCHAR)
                        ? (size_t) ((FixcharMeta*) self->meta)->n
                        : stype_info[stype].elemsize;

    // Create the new Column object.
    Column *res = make_data_column(stype, 0);
    res->nrows = (int64_t) nrows;

    // "Slice" rowindex with step = 1 is a simple subsection of the column
    if (rowindex->type == RI_SLICE && rowindex->slice.step == 1) {
        size_t start = (size_t) rowindex->slice.start;
        switch (stype) {
            case ST_STRING_I4_VCHAR: {
                size_t offoff = (size_t)((VarcharMeta*) self->meta)->offoff;
                int32_t *offs = (int32_t*) add_ptr(self->data, offoff) + start;
                int32_t off0 = start? abs(*(offs - 1)) - 1 : 0;
                int32_t off1 = start + nrows? abs(*(offs + nrows - 1)) - 1 : 0;
                size_t datasize = (size_t)(off1 - off0);
                size_t padding = column_i4s_padding(datasize);
                size_t offssize = nrows * elemsize;
                offoff = datasize + padding;
                res->alloc_size = datasize + padding + offssize;
                dtmalloc(res->data, void, res->alloc_size);
                ((VarcharMeta*) res->meta)->offoff = (int64_t)offoff;
                memcpy(res->data, add_ptr(self->data, off0), datasize);
                memset(add_ptr(res->data, datasize), 0xFF, padding);
                int32_t *resoffs = (int32_t*) add_ptr(res->data, offoff);
                for (size_t i = 0; i < nrows; i++) {
                    resoffs[i] = offs[i] > 0? offs[i] - off0
                                            : offs[i] + off0;
                }
            } break;

            case ST_STRING_I8_VCHAR: {
                size_t offoff = (size_t)((VarcharMeta*) self->meta)->offoff;
                int64_t *offs = (int64_t*) add_ptr(self->data, offoff) + start;
                int64_t off0 = start? llabs(*(offs - 1)) - 1 : 0;
                int64_t off1 = start + nrows? llabs(*(offs + nrows - 1)) - 1 : 0;
                size_t datasize = (size_t)(off1 - off0);
                size_t padding = column_i8s_padding(datasize);
                size_t offssize = nrows * elemsize;
                offoff = datasize + padding;
                res->alloc_size = datasize + padding + offssize;
                dtmalloc(res->data, void, res->alloc_size);
                ((VarcharMeta*) res->meta)->offoff = (int64_t)offoff;
                memcpy(res->data, add_ptr(self->data, off0), datasize);
                memset(add_ptr(res->data, datasize), 0xFF, padding);
                int64_t *resoffs = (int64_t*) add_ptr(res->data, offoff);
                for (size_t i = 0; i < nrows; i++) {
                    resoffs[i] = offs[i] > 0? offs[i] - off0
                                            : offs[i] + off0;
                }
            } break;

            case ST_STRING_U1_ENUM:
            case ST_STRING_U2_ENUM:
            case ST_STRING_U4_ENUM:
                assert(0);  // not implemented yet
                break;

            default: {
                assert(!stype_info[stype].varwidth);
                size_t alloc_size = nrows * elemsize;
                size_t offset = start * elemsize;
                dtmalloc(res->data, void, alloc_size);
                memcpy(res->data, add_ptr(self->data, offset), alloc_size);
                res->alloc_size = alloc_size;
            } break;
        }
        return res;
    }

    // In all other cases we need to iterate through the rowindex and fetch
    // the required elements manually.
    switch (stype) {
        #define JINIT_SLICE                                                    \
            int64_t start = rowindex->slice.start;                             \
            int64_t step = rowindex->slice.step;                               \
            int64_t j = start - step;
        #define JITER_SLICE                                                    \
            j += step;
        #define JINIT_ARR(bits)                                                \
            intXX(bits) *rowindices = rowindex->ind ## bits;
        #define JITER_ARR(bits)                                                \
            intXX(bits) j = rowindices[i];

        #define CASE_IX_VCHAR_SUB(ctype, abs, JINIT, JITER) {                  \
            size_t offoff = (size_t)((VarcharMeta*) self->meta)->offoff;       \
            ctype *offs = (ctype*) add_ptr(self->data, offoff);                \
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
            size_t padding = elemsize == 4 ? column_i4s_padding(datasize)      \
                                           : column_i8s_padding(datasize);     \
            size_t offssize = nrows * elemsize;                                \
            offoff = datasize + padding;                                       \
            res->alloc_size = offoff + offssize;                               \
            res->data = (char*) TRY(malloc(res->alloc_size));                  \
            ((VarcharMeta*) res->meta)->offoff = (int64_t) offoff;             \
            {   JINIT                                                          \
                ctype lastoff = 1;                                             \
                char *dest = res->data;                                        \
                ctype *resoffs = (ctype*) add_ptr(res->data, offoff);          \
                for (size_t i = 0; i < nrows; i++) {                           \
                    JITER                                                      \
                    if (offs[j] > 0) {                                         \
                        ctype prevoff = j? abs(offs[j - 1]) : 1;               \
                        size_t len = (size_t)(offs[j] - prevoff);              \
                        if (len) {                                             \
                            memcpy(dest, add_ptr(self->data, prevoff - 1),     \
                                   len);                                       \
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
            if (rowindex->type == RI_SLICE)                                    \
                CASE_IX_VCHAR_SUB(ctype, abs, JINIT_SLICE, JITER_SLICE)        \
            else if (rowindex->type == RI_ARR32)                               \
                CASE_IX_VCHAR_SUB(ctype, abs, JINIT_ARR(32), JITER_ARR(32))    \
            else if (rowindex->type == RI_ARR64)                               \
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
            dtmalloc(res->data, void, alloc_size);
            res->alloc_size = alloc_size;
            char *dest = res->data;
            if (rowindex->type == RI_SLICE) {
                size_t startsize = (size_t)rowindex->slice.start * elemsize;
                size_t stepsize = (size_t) rowindex->slice.step * elemsize;
                char *src = add_ptr(self->data, startsize);
                for (size_t i = 0; i < nrows; i++) {
                    memcpy(dest, src, elemsize);
                    dest += elemsize;
                    src += stepsize;
                }
            } else
            if (rowindex->type == RI_ARR32) {
                int32_t *rowindices = rowindex->ind32;
                for (size_t i = 0; i < nrows; i++) {
                    size_t j = (size_t) rowindices[i];
                    memcpy(dest, add_ptr(self->data, j*elemsize), elemsize);
                    dest += elemsize;
                }
            } else
            if (rowindex->type == RI_ARR32) {
                int64_t *rowindices = rowindex->ind64;
                for (size_t i = 0; i < nrows; i++) {
                    size_t j = (size_t) rowindices[i];
                    memcpy(dest, add_ptr(self->data, j*elemsize), elemsize);
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



/**
 * Expand the column up to `nrows` elements and return it. The pointer returned
 * will be `self` if the column can be modified in-place, or a new Column object
 * otherwise. In the latter case the original object `self` will be decrefed.
 *
 * The column itself will be expanded in the following way: if it had just a
 * single row, then this value will be repeated `nrows` times. If the column had
 * `nrows != 1`, then all extra rows will be filled with NAs.
 */
Column* column_realloc_and_fill(Column *self, int64_t nrows)
{
    int64_t old_nrows = self->nrows;
    size_t diff_rows = (size_t)(nrows - old_nrows);
    size_t old_alloc_size = self->alloc_size;
    assert(diff_rows > 0);

    if (!stype_info[self->stype].varwidth) {
        size_t elemsize = stype_info[self->stype].elemsize;
        Column *col;
        // DATA column with refcount 1 can be expanded in-place
        if (self->mtype == MT_DATA && self->refcount == 1) {
            col = self;
            col->nrows = nrows;
            col->alloc_size = elemsize * (size_t)nrows;
            dtrealloc(col->data, void, col->alloc_size);
        }
        // In all other cases we create a new Column object and copy the data
        // over. The current Column can be decrefed.
        else {
            col = make_data_column(self->stype, (size_t)nrows);
            if (col == NULL) return NULL;
            memcpy(col->data, self->data, self->alloc_size);
            column_decref(self);
        }
        // Replicate the value or fill with NAs
        size_t fill_size = elemsize * diff_rows;
        assert(col->alloc_size - old_alloc_size == fill_size);
        if (old_nrows == 1) {
            set_value(add_ptr(col->data, old_alloc_size), col->data,
                      elemsize, diff_rows);
        } else {
            const void *na = stype_info[col->stype].na;
            set_value(add_ptr(col->data, old_alloc_size), na,
                      elemsize, diff_rows);
        }
        return col;
    }

    else if (self->stype == ST_STRING_I4_VCHAR) {
        if (nrows > INT32_MAX)
            dterrr("Nrows is too big for an i4s column: %lld", nrows);

        size_t old_data_size = column_i4s_datasize(self);
        size_t old_offoff = (size_t) ((VarcharMeta*) self->meta)->offoff;
        size_t new_data_size = old_data_size;
        if (old_nrows == 1) new_data_size = old_data_size * (size_t)nrows;
        size_t new_padding_size = column_i4s_padding(new_data_size);
        size_t new_offoff = new_data_size + new_padding_size;
        size_t new_alloc_size = new_offoff + 4 * (size_t)nrows;
        assert(new_alloc_size > old_alloc_size);
        Column *col;

        // DATA column with refcount 1: expand in-place
        if (self->mtype == MT_DATA && self->refcount == 1) {
            col = self;
            dtrealloc(col->data, void, new_alloc_size);
            if (old_offoff != new_offoff) {
                memmove(add_ptr(col->data, new_offoff),
                        add_ptr(col->data, old_offoff), 4 * old_nrows);
            }
        }
        // Otherwise create a new column and copy over the data
        else {
            col = make_data_column(self->stype, 0);
            dtrealloc(col->data, void, new_alloc_size);
            memcpy(col->data, self->data, old_data_size);
            memcpy(add_ptr(col->data, new_offoff),
                   add_ptr(self->data, old_offoff), 4 * old_nrows);
            column_decref(self);
        }
        set_value(add_ptr(col->data, new_data_size), NULL, 1, new_padding_size);
        ((VarcharMeta*) col->meta)->offoff = (int64_t) new_offoff;
        col->alloc_size = new_alloc_size;
        col->nrows = nrows;

        // Replicate the value, or fill with NAs
        int32_t *offsets = (int32_t*) add_ptr(col->data, new_offoff);
        if (old_nrows == 1 && offsets[0] > 0) {
            set_value(add_ptr(col->data, old_data_size), col->data,
                      old_data_size, diff_rows);
            for (int32_t j = 0; j < (int32_t)nrows; j++) {
                offsets[j] = 1 + (j + 1) * (int32_t)old_data_size;
            }
        } else {
            if (old_nrows == 1) assert(old_data_size == 0);
            assert(old_offoff == new_offoff && old_data_size == new_data_size);
            int32_t na = -(int32_t)new_data_size - 1;
            set_value(add_ptr(col->data, old_alloc_size),
                      &na, 4, diff_rows);
        }
        return col;
    }
    // Exception
    dterre("Cannot realloc column of stype %d", self->stype);
}



RowIndex* column_sort(Column *col, int64_t nrows)
{
    assert(col->nrows == nrows);
    int32_t *ordering = NULL;
    if (col->stype == ST_INTEGER_I4) {
        sort_i4(col->data, (int32_t)nrows, &ordering);
    } else
    if (col->stype == ST_INTEGER_I1 || col->stype == ST_BOOLEAN_I1) {
        sort_i1(col->data, (int32_t)nrows, &ordering);
    } else {
        dterrr("Cannot sort column of stype %d", col->stype);
    }
    if (!ordering) return NULL;
    return rowindex_from_i32_array(ordering, (int32_t)nrows);
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
        if (self->mtype == MT_XBUF) {
            PyBuffer_Release(self->pybuf);
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
size_t column_i8s_padding(size_t datasize) {
    return ((8 - (datasize & 7)) & 7) + 8;
}


/**
 * Return the size of the data part in a ST_STRING_I(4|8)_VCHAR column.
 */
size_t column_i4s_datasize(Column *self) {
    assert(self->stype == ST_STRING_I4_VCHAR);
    void *end = add_ptr(self->data, self->alloc_size);
    return (size_t) abs(((int32_t*) end)[-1]) - 1;
}
size_t column_i8s_datasize(Column *self) {
    assert(self->stype == ST_STRING_I8_VCHAR);
    void *end = add_ptr(self->data, self->alloc_size);
    return (size_t) llabs(((int64_t*) end)[-1]) - 1;
}
