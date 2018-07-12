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

// TODO: merge with types.h / types.cc


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
  T end = offsets[row];
  if (ISNA<T>(end)) return none();
  T start = offsets[row - 1] & ~GETNA<T>();
  auto len = static_cast<Py_ssize_t>(end - start);
  return PyUnicode_FromStringAndSize(str_col->strdata() + start, len);
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

  py_stype_formatters[int(SType::VOID)]    = stype_notimpl;
  py_stype_formatters[int(SType::BOOL)]    = stype_boolean_i8_tostring;
  py_stype_formatters[int(SType::INT8)]    = stype_integer_i8_tostring;
  py_stype_formatters[int(SType::INT16)]   = stype_integer_i16_tostring;
  py_stype_formatters[int(SType::INT32)]   = stype_integer_i32_tostring;
  py_stype_formatters[int(SType::INT64)]   = stype_integer_i64_tostring;
  py_stype_formatters[int(SType::FLOAT32)] = stype_real_f32_tostring;
  py_stype_formatters[int(SType::FLOAT64)] = stype_real_f64_tostring;
  py_stype_formatters[int(SType::DEC16)]   = stype_notimpl;
  py_stype_formatters[int(SType::DEC32)]   = stype_notimpl;
  py_stype_formatters[int(SType::DEC64)]   = stype_notimpl;
  py_stype_formatters[int(SType::STR32)]   = stype_vchar_T_tostring<uint32_t>;
  py_stype_formatters[int(SType::STR64)]   = stype_vchar_T_tostring<uint64_t>;
  py_stype_formatters[int(SType::FSTR)]    = stype_notimpl;
  py_stype_formatters[int(SType::CAT8)]    = stype_notimpl;
  py_stype_formatters[int(SType::CAT16)]   = stype_notimpl;
  py_stype_formatters[int(SType::CAT32)]   = stype_notimpl;
  py_stype_formatters[int(SType::DATE64)]  = stype_notimpl;
  py_stype_formatters[int(SType::TIME32)]  = stype_notimpl;
  py_stype_formatters[int(SType::DATE32)]  = stype_notimpl;
  py_stype_formatters[int(SType::DATE16)]  = stype_notimpl;
  py_stype_formatters[int(SType::OBJ)]     = stype_object_pyptr_tostring;

  return 1;
}
