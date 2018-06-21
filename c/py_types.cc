//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "py_types.h"
#include "py_utils.h"
#include "column.h"

PyObject* py_ltype_names[DT_LTYPES_COUNT];
PyObject* py_stype_names[DT_STYPES_COUNT];
PyObject* py_ltype_objs[DT_LTYPES_COUNT];
PyObject* py_stype_objs[DT_STYPES_COUNT];

stype_formatter* py_stype_formatters[DT_STYPES_COUNT];
size_t py_buffers_size;



PyObject* bool_to_py(int8_t x) {
  return incref(x == 0? Py_False : x == 1? Py_True : Py_None);
}

PyObject* int_to_py(int8_t x) {
  return ISNA<int8_t>(x)? none() : PyLong_FromLong(static_cast<long>(x));
}

PyObject* int_to_py(int16_t x) {
  return ISNA<int16_t>(x)? none() : PyLong_FromLong(static_cast<long>(x));
}

PyObject* int_to_py(int32_t x) {
  return ISNA<int32_t>(x)? none() : PyLong_FromLong(static_cast<long>(x));
}

PyObject* int_to_py(int64_t x) {
  return ISNA<int64_t>(x)? none() : PyLong_FromInt64(x);
}

PyObject* float_to_py(float x) {
  return ISNA<float>(x)? none() : PyFloat_FromDouble(static_cast<double>(x));
}

PyObject* float_to_py(double x) {
  return ISNA<double>(x)? none() : PyFloat_FromDouble(x);
}

PyObject* string_to_py(const CString& x) {
  return x.size < 0? none() : PyUnicode_FromStringAndSize(x.ch, x.size);
}


static PyObject* stype_boolean_i8_tostring(Column* col, int64_t row) {
  int8_t x = static_cast<const int8_t*>(col->data())[row];
  return x == 0? incref(Py_False) :
         x == 1? incref(Py_True) : none();
}

static PyObject* stype_integer_i8_tostring(Column* col, int64_t row) {
  int8_t x = static_cast<const int8_t*>(col->data())[row];
  return ISNA<int8_t>(x)? none() : PyLong_FromLong(x);
}

static PyObject* stype_integer_i16_tostring(Column* col, int64_t row) {
  int16_t x = static_cast<const int16_t*>(col->data())[row];
  return ISNA<int16_t>(x)? none() : PyLong_FromLong(x);
}

static PyObject* stype_integer_i32_tostring(Column* col, int64_t row) {
  int32_t x = static_cast<const int32_t*>(col->data())[row];
  return ISNA<int32_t>(x)? none() : PyLong_FromLong(x);
}

static PyObject* stype_integer_i64_tostring(Column* col, int64_t row) {
  int64_t x = static_cast<const int64_t*>(col->data())[row];
  return ISNA<int64_t>(x)? none() : PyLong_FromLongLong(x);
}

static PyObject* stype_real_f32_tostring(Column* col, int64_t row) {
  float x = static_cast<const float*>(col->data())[row];
  return ISNA<float>(x)? none() : PyFloat_FromDouble(static_cast<double>(x));
}

static PyObject* stype_real_f64_tostring(Column* col, int64_t row) {
  double x = static_cast<const double*>(col->data())[row];
  return ISNA<double>(x)? none() : PyFloat_FromDouble(x);
}


template <typename T>
static PyObject* stype_vchar_T_tostring(Column* col, int64_t row) {
  StringColumn<T>* str_col = static_cast<StringColumn<T>*>(col);
  const T* offsets = str_col->offsets();
  if (ISNA<T>(offsets[row]))
      return none();
  T start = offsets[row];
  T len = (offsets[row + 1] & StringColumn<T>::NONA) - start;
  return PyUnicode_FromStringAndSize(str_col->strdata() +
      static_cast<size_t>(start), len);
}

static PyObject* stype_object_pyptr_tostring(Column* col, int64_t row)
{
  PyObject* x = static_cast<PyObject* const*>(col->data())[row];
  Py_INCREF(x);
  return x;
}

static PyObject* stype_notimpl(Column* col, int64_t)
{
  PyErr_Format(PyExc_NotImplementedError,
               "Cannot stringify column of type %d", col->stype());
  return nullptr;
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
    if (py_ltype_names[i] == nullptr) return 0;
  }

  for (int i = 0; i < DT_STYPES_COUNT; i++) {
    py_stype_names[i] = PyUnicode_FromString(stype_info[i].code);
    if (py_stype_names[i] == nullptr) return 0;
  }

  py_stype_formatters[ST_VOID]               = stype_notimpl;
  py_stype_formatters[ST_BOOLEAN_I1]         = stype_boolean_i8_tostring;
  py_stype_formatters[ST_INTEGER_I1]         = stype_integer_i8_tostring;
  py_stype_formatters[ST_INTEGER_I2]         = stype_integer_i16_tostring;
  py_stype_formatters[ST_INTEGER_I4]         = stype_integer_i32_tostring;
  py_stype_formatters[ST_INTEGER_I8]         = stype_integer_i64_tostring;
  py_stype_formatters[ST_REAL_F4]            = stype_real_f32_tostring;
  py_stype_formatters[ST_REAL_F8]            = stype_real_f64_tostring;
  py_stype_formatters[ST_REAL_I2]            = stype_notimpl;
  py_stype_formatters[ST_REAL_I4]            = stype_notimpl;
  py_stype_formatters[ST_REAL_I8]            = stype_notimpl;
  py_stype_formatters[ST_STRING_I4_VCHAR]    = stype_vchar_T_tostring<uint32_t>;
  py_stype_formatters[ST_STRING_I8_VCHAR]    = stype_vchar_T_tostring<uint64_t>;
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
    if (py_stype_objs[i] == nullptr) {
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
    if (py_ltype_objs[i] == nullptr) {
      PyErr_Clear();
      py_ltype_objs[i] = none();
    }
  }
}
