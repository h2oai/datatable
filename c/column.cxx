#include <sys/mman.h>
#include <stdio.h>
#include <string.h>    // memcpy, strcmp
#include <cstdlib>     // atoll
#include <errno.h>
#include <fcntl.h>     // open
#include <unistd.h>    // close
#include "column.h"
#include "utils.h"
#include "myassert.h"
#include "rowindex.h"
#include "sort.h"
#include "py_utils.h"

extern void free_xbuf_column(Column *col);
extern void PyBuffer_Release(Py_buffer *view);
extern size_t py_buffers_size;

/**
 * Simple Column constructor: create a Column with memory type `MT_DATA` and
 * having the provided SType. The column will be preallocated for `nrows` rows,
 * and will have `refcount` = 1. For a variable-width column only the "fixed-
 * -width" part will be allocated.
 *
 * If the column cannot be created (probably due to Out-of-Memory exception),
 * the function will return NULL.
 */
Column::Column(SType st, size_t nr) :
    data(NULL),
    meta(NULL),
    nrows((int64_t) nr),
    alloc_size(0),
    filename(NULL),
    stats(Stats::void_ptr()),
    refcount(1),
    mtype(MT_DATA),
    stype(st),
    _padding(0) {
    alloc_size = stype_info[st].elemsize * nr +
                 (st == ST_STRING_I4_VCHAR ? i4s_padding(0) :
                  st == ST_STRING_I8_VCHAR ? i8s_padding(0) : 0);
    data = malloc(alloc_size);
    size_t meta_size = stype_info[st].metasize;
    if (meta_size) meta = malloc(meta_size);
    if (st == ST_STRING_I4_VCHAR)
        ((VarcharMeta*) meta)->offoff = (int64_t) i4s_padding(0);
    if (st == ST_STRING_I8_VCHAR)
        ((VarcharMeta*) meta)->offoff = (int64_t) i8s_padding(0);
}


Column::Column(SType st, size_t nr, const char* fn) :
    data(NULL),
    meta(NULL),
    nrows((int64_t) nr),
    alloc_size(0),
    filename(NULL),
    stats(Stats::void_ptr()),
    refcount(1),
    mtype(MT_MMAP),
    stype(st),
    _padding(0) {
    alloc_size = stype_info [st].elemsize * nr;

    // Create new file of size `alloc_size`.
    FILE *fp = fopen(fn, "w");
    fseek(fp, (off_t) alloc_size - 1 , SEEK_SET);
    fputc('\0', fp);
    fclose(fp);

    // Memory-map the file. For some reason I was not able to create memory map
    // without resorting to closing / reopening the file (even with fflush()),
    // it was producing Permission Denied error for some reason.
    int fd = open(fn, O_RDWR);
    void *mmp = mmap(NULL, alloc_size, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
    close(fd);

    if (mmp == MAP_FAILED) throw new Error("Memory-map failed.");
    meta = malloc(stype_info[st].metasize);
}



/**
 * Save this column's data buffer into file `filename`. Other information
 * about the column should be stored elsewhere (for example in the _meta.nff
 * file).
 * If a file with the given name already exists, it will be overwritten.
 */
Column* Column::save_to_disk(const char *filename) {
    // Open and memory-map the file
    int fd = open(filename, O_RDWR|O_CREAT, 0666);
    if (fd == -1) {
        close(fd);
        throw new Error("Cannot open file %s", filename);
    }
    int ret = ftruncate(fd, (off_t) alloc_size);
    if (ret == -1) {
        close(fd);
        throw new Error("Cannot truncate file %s", filename);
    }
    void *mmp = mmap(NULL, alloc_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (mmp == MAP_FAILED) throw new Error("Memory-map failed.");

    // Copy the data buffer into the file
    memcpy(mmp, data, alloc_size);
    munmap(mmp, alloc_size);
    return this;
}



/**
 * Restore a Column previously saved via `column_save_to_disk()`. The column's
 * data buffer is taken from the file `filename`; and the column is assumed to
 * have type `stype`, number of rows `nrows`, and its meta information stored
 * as a string `metastr`.
 * This function will not check data validity (i.e. that the buffer contains
 * valid values, and that the extra parameters match the buffer's contents).
 */
Column::Column(const char* fn, SType st, int64_t nr,
               const char* ms) :
    data(NULL),
    meta(NULL),
    nrows(nr),
    alloc_size(0),
    filename(NULL),
    stats(Stats::void_ptr()),
    refcount(1),
    mtype(MT_MMAP),
    stype(st),
    _padding(0) {
    // Deserialize the meta information, if needed
    if (st == ST_STRING_I4_VCHAR || st == ST_STRING_I8_VCHAR) {
        if (strncmp(ms, "offoff=", 7) != 0)
            throw new Error("Cannot retrieve required metadata in string \"%s\"", ms);
        int64_t offoff = (int64_t) atoll(ms + 7);
        meta = malloc(sizeof(VarcharMeta));
        ((VarcharMeta*) meta)->offoff = offoff;
    }

    // Open and memory-map the file
    int fd = open(fn, O_RDONLY);
    if (fd == -1) {
        close(fd);
        throw new Error("Cannot open file %s", fn);
    }
    struct stat stat_buf;
    int ret = fstat(fd, &stat_buf);
    if (ret == -1) {
        close(fd);
        throw new Error("Cannot obtain file's size");
    }
    size_t filesize = (size_t) stat_buf.st_size;
    void *mmp = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (mmp == MAP_FAILED) throw new Error("Memory-map failed");
    data = mmp;
    alloc_size = filesize;
}


/**
 * Construct a column from the externally provided buffer.
 */
Column::Column(SType st, int64_t nr, void* pybuffer,
               void* b, size_t a_size) :
    data(b),
    meta(NULL),
    nrows(nr),
    alloc_size(a_size),
    pybuf(pybuffer),
    stats(Stats::void_ptr()),
    refcount(1),
    mtype(MT_XBUF),
    stype(st),
    _padding(0) {}



/**
 * Make a "deep" copy of the column. The column created with this method will
 * have memory-type MT_DATA and refcount of 1.
 * If a shallow copy is needed, then simply copy the column's reference and
 * call `column_incref()`.
 */
Column::Column(const Column &other) :
    data(NULL),
    meta(NULL),
    nrows(other.nrows),
    alloc_size(other.alloc_size),
    filename(NULL),
    stats(Stats::void_ptr()), // TODO: deep copy stats when implemented
    refcount(1),
    mtype(MT_DATA),
    stype(other.stype),
    _padding(0) {
    if(alloc_size) {
        data = malloc(alloc_size);
        memcpy(data, other.data, alloc_size);
    }
    size_t meta_size = stype_info[stype].metasize;
    if (meta_size) {
        meta = malloc(meta_size);
        memcpy(meta, other.meta, meta_size);
    }
}

Column::Column(const Column *other) :
    Column(*other) {}


/**
 * Extract data from this column at rows specified in the provided `rowindex`.
 * A new Column with type `MT_DATA` will be created and returned. The
 * `rowindex` parameter can also be NULL, in which case a shallow copy
 * is returned (if a "deep" copy is needed, then use `column_copy()`).
 */
Column* Column::extract(RowIndex *rowindex) {
    // If `rowindex` is not provided, then return a shallow "copy".
    if (rowindex == NULL) {
        return incref();
    }

    size_t res_nrows = (size_t) rowindex->length;
    size_t elemsize = (stype == ST_STRING_FCHAR)
                        ? (size_t) ((FixcharMeta*) meta)->n
                        : stype_info[stype].elemsize;

    // Create the new Column object.
    // TODO: Stats should be copied from DataTable
    Column *res = new Column(stype, 0);
    res->nrows = (int64_t) res_nrows;

    // "Slice" rowindex with step = 1 is a simple subsection of the column
    if (rowindex->type == RI_SLICE && rowindex->slice.step == 1) {
        size_t start = (size_t) rowindex->slice.start;
        switch (stype) {
            case ST_STRING_I4_VCHAR: {
                size_t offoff = (size_t)((VarcharMeta*) meta)->offoff;
                int32_t *offs = (int32_t*) add_ptr(data, offoff) + start;
                int32_t off0 = start ? abs(*(offs - 1)) - 1 : 0;
                int32_t off1 = start + res_nrows ?
                               abs(*(offs + res_nrows - 1)) - 1 : 0;
                size_t datasize = (size_t)(off1 - off0);
                size_t padding = i4s_padding(datasize);
                size_t offssize = res_nrows * elemsize;
                offoff = datasize + padding;
                res->alloc_size = datasize + padding + offssize;
                res->data = malloc(res->alloc_size);
                ((VarcharMeta*) res->meta)->offoff = (int64_t)offoff;
                memcpy(res->data, add_ptr(data, off0), datasize);
                memset(add_ptr(res->data, datasize), 0xFF, padding);
                int32_t *resoffs = (int32_t*) add_ptr(res->data, offoff);
                for (size_t i = 0; i < res_nrows; ++i) {
                    resoffs[i] = offs[i] > 0? offs[i] - off0
                                            : offs[i] + off0;
                }
            } break;

            case ST_STRING_I8_VCHAR: {
                size_t offoff = (size_t)((VarcharMeta*) meta)->offoff;
                int64_t *offs = (int64_t*) add_ptr(data, offoff) + start;
                int64_t off0 = start ? llabs(*(offs - 1)) - 1 : 0;
                int64_t off1 = start + res_nrows ?
                               llabs(*(offs + res_nrows - 1)) - 1 : 0;
                size_t datasize = (size_t)(off1 - off0);
                size_t padding = i8s_padding(datasize);
                size_t offssize = res_nrows * elemsize;
                offoff = datasize + padding;
                res->alloc_size = datasize + padding + offssize;
                res->data = malloc(res->alloc_size);
                ((VarcharMeta*) res->meta)->offoff = (int64_t)offoff;
                memcpy(res->data, add_ptr(data, off0), datasize);
                memset(add_ptr(res->data, datasize), 0xFF, padding);
                int64_t *resoffs = (int64_t*) add_ptr(res->data, offoff);
                for (size_t i = 0; i < res_nrows; ++i) {
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
                size_t res_alloc_size = res_nrows * elemsize;
                size_t offset = start * elemsize;
                dtmalloc(res->data, void, res_alloc_size);
                memcpy(res->data, add_ptr(data, offset), res_alloc_size);
                res->alloc_size = res_alloc_size;
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
            size_t offoff = (size_t)((VarcharMeta*) meta)->offoff;             \
            ctype *offs = (ctype*) add_ptr(data, offoff);                      \
            size_t datasize = 0;                                               \
            {   JINIT                                                          \
                for (size_t i = 0; i < res_nrows; ++i) {                       \
                    JITER                                                      \
                    if (offs[j] > 0) {                                         \
                        ctype prevoff = j? abs(offs[j - 1]) : 1;               \
                        datasize += (size_t)(offs[j] - prevoff);               \
                    }                                                          \
                }                                                              \
            }                                                                  \
            size_t padding = elemsize == 4 ? i4s_padding(datasize)      \
                                           : i8s_padding(datasize);     \
            size_t offssize = res_nrows * elemsize;                            \
            offoff = datasize + padding;                                       \
            res->alloc_size = offoff + offssize;                               \
            res->data = TRY(malloc(res->alloc_size));                          \
            ((VarcharMeta*) res->meta)->offoff = (int64_t) offoff;             \
            {   JINIT                                                          \
                ctype lastoff = 1;                                             \
                char *dest = (char*) res->data;                                \
                ctype *resoffs = (ctype*) add_ptr(res->data, offoff);          \
                for (size_t i = 0; i < res_nrows; ++i) {                       \
                    JITER                                                      \
                    if (offs[j] > 0) {                                         \
                        ctype prevoff = j? abs(offs[j - 1]) : 1;               \
                        size_t len = (size_t)(offs[j] - prevoff);              \
                        if (len) {                                             \
                            memcpy(dest, add_ptr(data, prevoff - 1),           \
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
            size_t res_alloc_size = res_nrows * elemsize;
            res->data = malloc(res_alloc_size);
            res->alloc_size = res_alloc_size;
            char *dest = (char*) res->data;
            if (rowindex->type == RI_SLICE) {
                size_t startsize = (size_t) rowindex->slice.start * elemsize;
                size_t stepsize = (size_t) rowindex->slice.step * elemsize;
                char *src = (char*)(data) + startsize;
                for (size_t i = 0; i < res_nrows; ++i) {
                    memcpy(dest, src, elemsize);
                    dest += elemsize;
                    src += stepsize;
                }
            } else
            if (rowindex->type == RI_ARR32) {
                int32_t *rowindices = rowindex->ind32;
                for (size_t i = 0; i < res_nrows; ++i) {
                    size_t j = (size_t) rowindices[i];
                    memcpy(dest, add_ptr(data, j*elemsize), elemsize);
                    dest += elemsize;
                }
            } else
            if (rowindex->type == RI_ARR32) {
                int64_t *rowindices = rowindex->ind64;
                for (size_t i = 0; i < res_nrows; ++i) {
                    size_t j = (size_t) rowindices[i];
                    memcpy(dest, add_ptr(data, j*elemsize), elemsize);
                    dest += elemsize;
                }
            }
        } break;
    }
    return res;

  fail:
    res->decref();
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
Column* Column::realloc_and_fill(int64_t nr)
{
    size_t old_nrows = (size_t) this->nrows;
    size_t diff_rows = (size_t) nr - old_nrows;
    size_t old_alloc_size = this->alloc_size;
    assert(diff_rows > 0);

    if (!stype_info[stype].varwidth) {
        size_t elemsize = stype_info[stype].elemsize;
        Column *col;
        // DATA column with refcount 1 can be expanded in-place
        if (mtype == MT_DATA && refcount == 1) {
            col = this;
            col->nrows = nr;
            col->alloc_size = elemsize * (size_t) nr;
            dtrealloc(col->data, void, col->alloc_size);
        }
        // In all other cases we create a new Column object and copy the data
        // over. The current Column can be decrefed.
        else {
            col = new Column(stype, (size_t) nr);
            memcpy(col->data, data, alloc_size);
            decref();
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
        // TODO: Temporary fix. To be resolved in #301
        col->stats->reset();
        return col;
    }

    else if (stype == ST_STRING_I4_VCHAR) {
        if (nr > INT32_MAX)
            dterrr("Nrows is too big for an i4s column: %lld", nr);

        size_t old_data_size = i4s_datasize();
        size_t old_offoff = (size_t) ((VarcharMeta*) meta)->offoff;
        size_t new_data_size = old_data_size;
        if (old_nrows == 1) new_data_size = old_data_size * (size_t) nr;
        size_t new_padding_size = Column::i4s_padding(new_data_size);
        size_t new_offoff = new_data_size + new_padding_size;
        size_t new_alloc_size = new_offoff + 4 * (size_t) nr;
        assert(new_alloc_size > old_alloc_size);
        Column *col;

        // DATA column with refcount 1: expand in-place
        if (mtype == MT_DATA && refcount == 1) {
            col = this;
            dtrealloc(col->data, void, new_alloc_size);
            if (old_offoff != new_offoff) {
                memmove(add_ptr(col->data, new_offoff),
                        add_ptr(col->data, old_offoff), 4 * old_nrows);
            }
        }
        // Otherwise create a new column and copy over the data
        else {
            col = new Column(stype, 0);
            dtrealloc(col->data, void, new_alloc_size);
            memcpy(col->data, data, old_data_size);
            memcpy(add_ptr(col->data, new_offoff),
                   add_ptr(data, old_offoff), 4 * old_nrows);
            decref();
        }
        set_value(add_ptr(col->data, new_data_size), NULL, 1, new_padding_size);
        ((VarcharMeta*) col->meta)->offoff = (int64_t) new_offoff;
        col->alloc_size = new_alloc_size;
        col->nrows = nr;

        // Replicate the value, or fill with NAs
        int32_t *offsets = (int32_t*) add_ptr(col->data, new_offoff);
        if (old_nrows == 1 && offsets[0] > 0) {
            set_value(add_ptr(col->data, old_data_size), col->data,
                      old_data_size, diff_rows);
            for (int32_t j = 0; j < (int32_t) nr; ++j) {
                offsets[j] = 1 + (j + 1) * (int32_t) old_data_size;
            }
        } else {
            if (old_nrows == 1) assert(old_data_size == 0);
            assert(old_offoff == new_offoff && old_data_size == new_data_size);
            int32_t na = -(int32_t) new_data_size - 1;
            set_value(add_ptr(col->data, old_alloc_size),
                      &na, 4, diff_rows);
        }
        // TODO: Temporary fix. To be resolved in #301
        col->stats->reset();
        return col;
    }
    // Exception
    throw new Error("Cannot realloc column of stype %d", stype);
}




/**
 * Increase reference count on column `self`. This function should be called
 * when a new long-term copy to the column is created (for example if the column
 * is referenced from several data tables). For convenience, this function
 * returns the column object passed.
 * Here `self` can also be NULL, in which case this function does nothing.
 */
Column* Column::incref() {
    ++refcount;
    return this;
}



/**
 * Decrease reference count on column `self`. Call this function when disposing
 * of a previously held pointer to a column. If the internal reference counter
 * reaches 0, the column's data will be garbage-collected.
 * Here `self` can also be NULL, in which case this function does nothing.
 */
void Column::decref() {
    --refcount;
    if (refcount <= 0) {
        switch(mtype) {
        case MT_DATA:
            dtfree(data);
            break;
        case MT_MMAP:
            munmap(data, alloc_size);
            break;
        case MT_XBUF:
            PyBuffer_Release((Py_buffer*) pybuf);
            break;
        default:
            break;
        }
        dtfree(meta)
        Stats::destruct(stats);
        delete this;
    }
}



/**
 * Compute the amount of padding between the data and offset section for an
 * ST_STRING_I4_VCHAR column. The formula ensures that datasize + padding are
 * always 8-byte aligned, and that the amount of padding is at least 4 bytes.
 */
size_t Column::i4s_padding(size_t datasize) {
    return ((8 - ((datasize + 4) & 7)) & 7) + 4;
}
size_t Column::i8s_padding(size_t datasize) {
    return ((8 - (datasize & 7)) & 7) + 8;
}


/**
 * Return the size of the data part in a ST_STRING_I(4|8)_VCHAR column.
 */
size_t Column::i4s_datasize() {
    assert(stype == ST_STRING_I4_VCHAR);
    void *end = add_ptr(data, alloc_size);
    return (size_t) abs(((int32_t*) end)[-1]) - 1;
}
size_t Column::i8s_datasize() {
    assert(stype == ST_STRING_I8_VCHAR);
    void *end = add_ptr(data, alloc_size);
    return (size_t) llabs(((int64_t*) end)[-1]) - 1;
}

/**
 * Get the total size of the memory occupied by this Column. This is different
 * from `column->alloc_size`, which in general reports byte size of the `data`
 * portion of the column.
 */
size_t Column::get_allocsize()
{
    size_t sz = sizeof(Column);
    switch (mtype) {
        case MT_MMAP:
        case MT_TEMP:
            if (filename)
                sz += strlen(filename) + 1;  // +1 for trailing '\0'
            [[clang::fallthrough]];
        case MT_DATA:
            sz += alloc_size;
            break;
        case MT_XBUF:
            sz += py_buffers_size;
            break;
    }
    sz += stype_info[stype].metasize;
    return sz;
}

