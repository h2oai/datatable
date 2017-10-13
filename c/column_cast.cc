#include <string.h>    // memcpy
#include "column.h"
#include "myassert.h"
#include "utils.h"

static castfn_ptr softcasts[DT_STYPES_COUNT][DT_STYPES_COUNT];
static castfn_ptr hardcasts[DT_STYPES_COUNT][DT_STYPES_COUNT];


/**
 * Convert column `self` into type `stype`. The conversion does not happen
 * in-place, instead a new Column object is always created. If `stype` is the
 * same as the current Column's `stype` then a plain clone will be returned.
 *
 * If the requested conversion has not been implemented yet, an exception will
 * be raised and NULL pointer returned.
 */
Column* Column::cast(SType new_stype) const
{
    castfn_ptr converter = hardcasts[stype()][new_stype];
    if (converter) {
        Column *res = Column::new_data_column(new_stype, this->nrows);
        return converter(this, res);
    } else if (stype() == new_stype) {
        return this->shallowcopy();
    } else {
        dterrv("Unable to cast from stype=%d into stype=%d", stype(), new_stype);
    }
}




//---- ST_BOOLEAN_I1 -----------------------------------------------------------

static Column* easy_i1b_to_i1i(const Column *self, Column *res)
{
    memcpy(res->data(), self->data(), self->alloc_size());
    return res;
}


//---- ST_INTEGER_I1 -----------------------------------------------------------

static Column* easy_i1i_to_i2i(const Column *self, Column *res)
{
    int8_t *src_data = (int8_t*) self->data();
    int16_t *res_data = (int16_t*) res->data();
    for (int64_t i = 0; i < self->nrows; i++) {
        int8_t x = src_data[i];
        res_data[i] = x == NA_I1? NA_I2 : x;
    }
    return res;
}

static Column* easy_i1i_to_i4i(const Column *self, Column *res)
{
    int8_t *src_data = (int8_t*) self->data();
    int32_t *res_data = (int32_t*) res->data();
    for (int64_t i = 0; i < self->nrows; i++) {
        int8_t x = src_data[i];
        res_data[i] = x == NA_I1? NA_I4 : x;
    }
    return res;
}

static Column* easy_i1i_to_i8i(const Column *self, Column *res)
{
    int8_t *src_data = (int8_t*) self->data();
    int64_t *res_data = (int64_t*) res->data();
    for (int64_t i = 0; i < self->nrows; i++) {
        int8_t x = src_data[i];
        res_data[i] = x == NA_I1? NA_I8 : x;
    }
    return res;
}

static Column* easy_i1i_to_f4r(const Column *self, Column *res)
{
    int8_t *src_data = (int8_t*) self->data();
    float *res_data = (float*) res->data();
    for (int64_t i = 0; i < self->nrows; i++) {
        int8_t x = src_data[i];
        res_data[i] = x == NA_I1? NA_F4 : x;
    }
    return res;
}

static Column* easy_i1i_to_f8r(const Column *self, Column *res)
{
    int8_t *src_data = (int8_t*) self->data();
    double *res_data = (double*) res->data();
    for (int64_t i = 0; i < self->nrows; i++) {
        int8_t x = src_data[i];
        res_data[i] = x == NA_I1? NA_F8 : x;
    }
    return res;
}


//---- ST_INTEGER_I2 -----------------------------------------------------------

static Column* easy_i2i_to_i4i(const Column *self, Column *res)
{
    int16_t *src_data = (int16_t*) self->data();
    int32_t *res_data = (int32_t*) res->data();
    for (int64_t i = 0; i < self->nrows; i++) {
        int16_t x = src_data[i];
        res_data[i] = x == NA_I2 ? NA_I4 : x;
    }
    return res;
}

static Column* easy_i2i_to_i8i(const Column *self, Column *res)
{
    int16_t *src_data = (int16_t*) self->data();
    int64_t *res_data = (int64_t*) res->data();
    for (int64_t i = 0; i < self->nrows; i++) {
        int16_t x = src_data[i];
        res_data[i] = x == NA_I2 ? NA_I8 : x;
    }
    return res;
}

static Column* easy_i2i_to_f4r(const Column *self, Column *res)
{
    int16_t *src_data = (int16_t*) self->data();
    float *res_data = (float*) res->data();
    for (int64_t i = 0; i < self->nrows; i++) {
        int16_t x = src_data[i];
        res_data[i] = x == NA_I2 ? NA_F4 : x;
    }
    return res;
}

static Column* easy_i2i_to_f8r(const Column *self, Column *res)
{
    int16_t *src_data = (int16_t*) self->data();
    double *res_data = (double*) res->data();
    for (int64_t i = 0; i < self->nrows; i++) {
        int16_t x = src_data[i];
        res_data[i] = x == NA_I2 ? NA_F8 : x;
    }
    return res;
}


//---- ST_INTEGER_I4 -----------------------------------------------------------

static Column* easy_i4i_to_i8i(const Column *self, Column *res)
{
    int32_t *src_data = (int32_t*) self->data();
    int64_t *res_data = (int64_t*) res->data();
    for (int64_t i = 0; i < self->nrows; i++) {
        int32_t x = src_data[i];
        res_data[i] = x == NA_I4 ? NA_I8 : x;
    }
    return res;
}

static Column* easy_i4i_to_f4r(const Column *self, Column *res)
{
    int32_t *src_data = (int32_t*) self->data();
    float *res_data = (float*) res->data();
    for (int64_t i = 0; i < self->nrows; i++) {
        int32_t x = src_data[i];
        res_data[i] = x == NA_I4 ? NA_F4 : x;
    }
    return res;
}

static Column* easy_i4i_to_f8r(const Column *self, Column *res)
{
    int32_t *src_data = (int32_t*) self->data();
    double *res_data = (double*) res->data();
    for (int64_t i = 0; i < self->nrows; i++) {
        int32_t x = src_data[i];
        res_data[i] = x == NA_I4 ? NA_F8 : x;
    }
    return res;
}


//---- ST_INTEGER_I8 -----------------------------------------------------------

static Column* easy_i8i_to_f4r(const Column *self, Column *res)
{
    int64_t *src_data = (int64_t*) self->data();
    float *res_data = (float*) res->data();
    for (int64_t i = 0; i < self->nrows; i++) {
        int64_t x = src_data[i];
        res_data[i] = x == NA_I8 ? NA_F4 : x;
    }
    return res;
}

static Column* easy_i8i_to_f8r(const Column *self, Column *res)
{
    int64_t *src_data = (int64_t*) self->data();
    double *res_data = (double*) res->data();
    for (int64_t i = 0; i < self->nrows; i++) {
        int64_t x = src_data[i];
        res_data[i] = x == NA_I8 ? NA_F8 : x;
    }
    return res;
}


//---- ST_REAL_F4 --------------------------------------------------------------

static Column* easy_f4r_to_f8r(const Column *self, Column *res)
{
    float *src_data = (float*) self->data();
    double *res_data = (double*) res->data();
    for (int64_t i = 0; i < self->nrows; i++) {
        float x = src_data[i];
        res_data[i] = ISNA_F4(x) ? NA_F8 : (double) x;
    }
    return res;
}




/**
 * This function has to be called only once, during module initialization.
 */
void init_column_cast_functions(void)
{
    for (int s = 0; s < DT_STYPES_COUNT; s++) {
        for (int t = 0; t < DT_STYPES_COUNT; t++) {
            softcasts[s][t] = NULL;
            hardcasts[s][t] = NULL;
        }
    }
    // Register all implemented converters
    hardcasts[ST_BOOLEAN_I1][ST_INTEGER_I1] = easy_i1b_to_i1i;
    hardcasts[ST_BOOLEAN_I1][ST_INTEGER_I2] = easy_i1i_to_i2i;
    hardcasts[ST_BOOLEAN_I1][ST_INTEGER_I4] = easy_i1i_to_i4i;
    hardcasts[ST_BOOLEAN_I1][ST_INTEGER_I8] = easy_i1i_to_i8i;
    hardcasts[ST_BOOLEAN_I1][ST_REAL_F4] = easy_i1i_to_f4r;
    hardcasts[ST_BOOLEAN_I1][ST_REAL_F8] = easy_i1i_to_f8r;

    hardcasts[ST_INTEGER_I1][ST_INTEGER_I2] = easy_i1i_to_i2i;
    hardcasts[ST_INTEGER_I1][ST_INTEGER_I4] = easy_i1i_to_i4i;
    hardcasts[ST_INTEGER_I1][ST_INTEGER_I8] = easy_i1i_to_i8i;
    hardcasts[ST_INTEGER_I1][ST_REAL_F4] = easy_i1i_to_f4r;
    hardcasts[ST_INTEGER_I1][ST_REAL_F8] = easy_i1i_to_f8r;

    hardcasts[ST_INTEGER_I2][ST_INTEGER_I4] = easy_i2i_to_i4i;
    hardcasts[ST_INTEGER_I2][ST_INTEGER_I8] = easy_i2i_to_i8i;
    hardcasts[ST_INTEGER_I2][ST_REAL_F4] = easy_i2i_to_f4r;
    hardcasts[ST_INTEGER_I2][ST_REAL_F8] = easy_i2i_to_f8r;

    hardcasts[ST_INTEGER_I4][ST_INTEGER_I8] = easy_i4i_to_i8i;
    hardcasts[ST_INTEGER_I4][ST_REAL_F4] = easy_i4i_to_f4r;
    hardcasts[ST_INTEGER_I4][ST_REAL_F8] = easy_i4i_to_f8r;

    hardcasts[ST_INTEGER_I8][ST_REAL_F4] = easy_i8i_to_f4r;
    hardcasts[ST_INTEGER_I8][ST_REAL_F8] = easy_i8i_to_f8r;

    hardcasts[ST_REAL_F4][ST_REAL_F8] = easy_f4r_to_f8r;

    // Initialize cast functions from/into py-objects
    init_column_cast_functions2(hardcasts);
}
