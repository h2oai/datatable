#include <Python.h>
#include "py_column.h"
#include "py_types.h"
#include "py_utils.h"


static Column* easy_i1b_to_p8p(Column *self, Column *res)
{
    int8_t *src_data = (int8_t*) self->data;
    PyObject **res_data = (PyObject**) res->data;
    for (int64_t i = 0; i < self->nrows; i++) {
        int8_t x = src_data[i];
        res_data[i] = x == 1? Py_True : x == 0? Py_False : Py_None;
        Py_INCREF(res_data[i]);
    }
    return res;
}


static Column* easy_i1i_to_p8p(Column *self, Column *res)
{
    int8_t *src_data = (int8_t*) self->data;
    PyObject **res_data = (PyObject**) res->data;
    for (int64_t i = 0; i < self->nrows; i++) {
        int8_t x = src_data[i];
        res_data[i] = ISNA_I1(x) ? none() : PyLong_FromLong(x);
    }
    return res;
}


static Column* easy_i2i_to_p8p(Column *self, Column *res)
{
    int16_t *src_data = (int16_t*) self->data;
    PyObject **res_data = (PyObject**) res->data;
    for (int64_t i = 0; i < self->nrows; i++) {
        int16_t x = src_data[i];
        res_data[i] = ISNA_I2(x) ? none() : PyLong_FromLong(x);
    }
    return res;
}


static Column* easy_i4i_to_p8p(Column *self, Column *res)
{
    int32_t *src_data = (int32_t*) self->data;
    PyObject **res_data = (PyObject**) res->data;
    for (int64_t i = 0; i < self->nrows; i++) {
        int32_t x = src_data[i];
        res_data[i] = ISNA_I4(x) ? none() : PyLong_FromLong(x);
    }
    return res;
}


static Column* easy_i8i_to_p8p(Column *self, Column *res)
{
    int64_t *src_data = (int64_t*) self->data;
    PyObject **res_data = (PyObject**) res->data;
    for (int64_t i = 0; i < self->nrows; i++) {
        int64_t x = src_data[i];
        res_data[i] = ISNA_I8(x) ? none() : PyLong_FromLong(x);
    }
    return res;
}


static Column* easy_f4r_to_p8p(Column *self, Column *res)
{
    float *src_data = (float*) self->data;
    PyObject **res_data = (PyObject**) res->data;
    for (int64_t i = 0; i < self->nrows; i++) {
        float x = src_data[i];
        res_data[i] = ISNA_F4(x) ? none() : PyFloat_FromDouble((double) x);
    }
    return res;
}


static Column* easy_f8r_to_p8p(Column *self, Column *res)
{
    double *src_data = (double*) self->data;
    PyObject **res_data = (PyObject**) res->data;
    for (int64_t i = 0; i < self->nrows; i++) {
        double x = src_data[i];
        res_data[i] = ISNA_F8(x) ? none() : PyFloat_FromDouble(x);
    }
    return res;
}


static Column* easy_i4s_to_p8p(Column *self, Column *res)
{
    char *strdata = (char*) self->data;
    int64_t offoff = ((VarcharMeta*) self->meta)->offoff;
    int32_t *offsets = (int32_t*) add_ptr(self->data, offoff);
    PyObject **res_data = (PyObject**) res->data;
    int32_t prev_off = 1;
    for (int64_t i = 0; i < self->nrows; i++) {
        int32_t off = offsets[i];
        if (off < 0) {
            res_data[i] = none();
        } else {
            int32_t len = off - prev_off;
            res_data[i] =
                PyUnicode_FromStringAndSize(strdata + prev_off - 1, len);
            prev_off = off;
        }
    }
    return res;
}


void init_column_cast_functions2(castfn_ptr hardcasts[][DT_STYPES_COUNT])
{
    hardcasts[ST_BOOLEAN_I1][ST_OBJECT_PYPTR] = easy_i1b_to_p8p;
    hardcasts[ST_INTEGER_I1][ST_OBJECT_PYPTR] = easy_i1i_to_p8p;
    hardcasts[ST_INTEGER_I2][ST_OBJECT_PYPTR] = easy_i2i_to_p8p;
    hardcasts[ST_INTEGER_I4][ST_OBJECT_PYPTR] = easy_i4i_to_p8p;
    hardcasts[ST_INTEGER_I8][ST_OBJECT_PYPTR] = easy_i8i_to_p8p;
    hardcasts[ST_REAL_F4][ST_OBJECT_PYPTR] = easy_f4r_to_p8p;
    hardcasts[ST_REAL_F8][ST_OBJECT_PYPTR] = easy_f8r_to_p8p;
    hardcasts[ST_STRING_I4_VCHAR][ST_OBJECT_PYPTR] = easy_i4s_to_p8p;
}
