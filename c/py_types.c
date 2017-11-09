#include "py_types.h"
#include "py_utils.h"

PyObject* py_ltype_names[DT_LTYPES_COUNT];
PyObject* py_stype_names[DT_STYPES_COUNT];
PyObject* py_ltype_objs[DT_LTYPES_COUNT];
PyObject* py_stype_objs[DT_STYPES_COUNT];

stype_formatter* py_stype_formatters[DT_STYPES_COUNT];
size_t py_buffers_size;


static PyObject* stype_boolean_i8_tostring(Column *col, int64_t row)
{
    int8_t x = ((int8_t*)col->data())[row];
    return x == 0? incref(Py_False) :
           x == 1? incref(Py_True) : none();
}

static PyObject* stype_integer_i8_tostring(Column *col, int64_t row)
{
    int8_t x = ((int8_t*)col->data())[row];
    return x == NA_I1? none() : PyLong_FromLong(x);
}

static PyObject* stype_integer_i16_tostring(Column *col, int64_t row)
{
    int16_t x = ((int16_t*)col->data())[row];
    return x == NA_I2? none() : PyLong_FromLong(x);
}

static PyObject* stype_integer_i32_tostring(Column *col, int64_t row)
{
    int32_t x = ((int32_t*)col->data())[row];
    return x == NA_I4? none() : PyLong_FromLong(x);
}

static PyObject* stype_integer_i64_tostring(Column *col, int64_t row)
{
    int64_t x = ((int64_t*)col->data())[row];
    return x == NA_I8? none() : PyLong_FromLongLong(x);
}

static PyObject* stype_real_f32_tostring(Column *col, int64_t row)
{
    float x = ((float*)col->data())[row];
    return ISNA_F4(x)? none() : PyFloat_FromDouble((double)x);
}

static PyObject* stype_real_f64_tostring(Column *col, int64_t row)
{
    double x = ((double*)col->data())[row];
    return ISNA_F8(x)? none() : PyFloat_FromDouble(x);
}

static PyObject* stype_real_i16_tostring(UNUSED(Column *col), UNUSED(int64_t row))
{
    /*DecimalMeta *meta = (DecimalMeta*) col->meta;
    int16_t x = ((int16_t*)col->data())[row];
    if (x == NA_I2) return none();
    double s = pow(10, meta->scale);
    return PyFloat_FromDouble(x / s);*/
    return none();
}

static PyObject* stype_real_i32_tostring(UNUSED(Column *col), UNUSED(int64_t row))
{
    /*DecimalMeta *meta = (DecimalMeta*) col->meta;
    int32_t x = ((int32_t*)col->data())[row];
    if (x == NA_I4) return none();
    double s = pow(10, meta->scale);
    return PyFloat_FromDouble(x / s);*/
    return none();
}

static PyObject* stype_real_i64_tostring(UNUSED(Column *col), UNUSED(int64_t row))
{
    /*DecimalMeta *meta = (DecimalMeta*) col->meta;
    int64_t x = ((int64_t*)col->data())[row];
    if (x == NA_I8) return none();
    double s = pow(10, meta->scale);
    return PyFloat_FromDouble(x / s);*/
    return none();
}

static PyObject* stype_vchar_i32_tostring(Column *col, int64_t row)
{
  StringColumn<int32_t>* str_col = static_cast<StringColumn<int32_t>*>(col);
  int32_t* offsets = str_col->offsets();
  if (offsets[row] < 0)
      return none();
  int32_t start = abs(offsets[row - 1]);
  int32_t len = offsets[row] - start;
  return PyUnicode_FromStringAndSize(str_col->strdata() +
      static_cast<size_t>(start), len);
}

static PyObject* stype_object_pyptr_tostring(Column *col, int64_t row)
{
    return incref(((PyObject**)col->data())[row]);
}

static PyObject* stype_notimpl(Column *col, UNUSED(int64_t row))
{
    PyErr_Format(PyExc_NotImplementedError,
                 "Cannot stringify column of type %d", col->stype());
    return NULL;
}


int init_py_types(PyObject*)
{
    init_types();
    py_buffers_size = sizeof(Py_buffer);

    py_ltype_names[LT_MU]       = PyUnicode_FromString("mu");
    py_ltype_names[LT_BOOLEAN]  = PyUnicode_FromString("bool");
    py_ltype_names[LT_INTEGER]  = PyUnicode_FromString("int");
    py_ltype_names[LT_REAL]     = PyUnicode_FromString("real");
    py_ltype_names[LT_STRING]   = PyUnicode_FromString("str");
    py_ltype_names[LT_DATETIME] = PyUnicode_FromString("time");
    py_ltype_names[LT_DURATION] = PyUnicode_FromString("duration");
    py_ltype_names[LT_OBJECT]   = PyUnicode_FromString("obj");

    for (int i = 0; i < DT_LTYPES_COUNT; i++) {
        if (py_ltype_names[i] == NULL) return 0;
    }

    for (int i = 0; i < DT_STYPES_COUNT; i++) {
        py_stype_names[i] = PyUnicode_FromString(stype_info[i].code);
        if (py_stype_names[i] == NULL) return 0;
    }

    py_stype_formatters[ST_VOID]               = stype_notimpl;
    py_stype_formatters[ST_BOOLEAN_I1]         = stype_boolean_i8_tostring;
    py_stype_formatters[ST_INTEGER_I1]         = stype_integer_i8_tostring;
    py_stype_formatters[ST_INTEGER_I2]         = stype_integer_i16_tostring;
    py_stype_formatters[ST_INTEGER_I4]         = stype_integer_i32_tostring;
    py_stype_formatters[ST_INTEGER_I8]         = stype_integer_i64_tostring;
    py_stype_formatters[ST_REAL_F4]            = stype_real_f32_tostring;
    py_stype_formatters[ST_REAL_F8]            = stype_real_f64_tostring;
    py_stype_formatters[ST_REAL_I2]            = stype_real_i16_tostring;
    py_stype_formatters[ST_REAL_I4]            = stype_real_i32_tostring;
    py_stype_formatters[ST_REAL_I8]            = stype_real_i64_tostring;
    py_stype_formatters[ST_STRING_I4_VCHAR]    = stype_vchar_i32_tostring;
    py_stype_formatters[ST_STRING_I8_VCHAR]    = stype_notimpl;
    py_stype_formatters[ST_STRING_FCHAR]       = stype_notimpl;
    py_stype_formatters[ST_STRING_U1_ENUM]     = stype_notimpl;
    py_stype_formatters[ST_STRING_U2_ENUM]     = stype_notimpl;
    py_stype_formatters[ST_STRING_U4_ENUM]     = stype_notimpl;
    py_stype_formatters[ST_DATETIME_I8_EPOCH]  = stype_notimpl;
    py_stype_formatters[ST_DATETIME_I4_TIME]   = stype_notimpl;
    py_stype_formatters[ST_DATETIME_I4_DATE]   = stype_notimpl;
    py_stype_formatters[ST_DATETIME_I2_MONTH]  = stype_notimpl;
    py_stype_formatters[ST_OBJECT_PYPTR]       = stype_object_pyptr_tostring;

    return 1;
}

void init_py_stype_objs(PyObject* stype_enum)
{
  for (int i = 0; i < DT_STYPES_COUNT; i++) {
    // The call may raise an exception -- that's ok
    py_stype_objs[i] = PyObject_CallFunction(stype_enum, "i", i);
    if (py_stype_objs[i] == NULL) {
      PyErr_Clear();
      py_stype_objs[i] = none();
    }
  }
}

void init_py_ltype_objs(PyObject* ltype_enum)
{
  for (int i = 0; i < DT_LTYPES_COUNT; i++) {
    // The call may raise an exception -- that's ok
    py_ltype_objs[i] = PyObject_CallFunction(ltype_enum, "i", i);
    if (py_ltype_objs[i] == NULL) {
      PyErr_Clear();
      py_ltype_objs[i] = none();
    }
  }
}
