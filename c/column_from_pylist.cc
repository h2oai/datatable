//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
#include "column.h"
#include <cstdlib>     // abs
#include <limits>      // numeric_limits
#include "py_types.h"  // PyLong_AsInt64AndOverflow
#include "utils.h"



static bool parse_as_bool(PyObject* list, MemoryBuffer* membuf, int64_t& from)
{
  int64_t nrows = Py_SIZE(list);
  membuf->resize(static_cast<size_t>(nrows));
  int8_t* outdata = static_cast<int8_t*>(membuf->get());

  // Use the fact that Python stores small integers as singletons, and thus
  // in order to check whether a PyObject* is integer 0 or 1 it's enough to
  // check whether the objects are the same.
  const PyObject* Py_One = PyLong_FromLong(1);
  const PyObject* Py_Zero = PyLong_FromLong(0);
  for (int64_t i = 0; i < nrows; i++) {
    PyObject* item = PyList_GET_ITEM(list, i);

    if (item == Py_None) {
      outdata[i] = NA_I1;
      continue;
    }
    else if (item == Py_True || item == Py_One) {
      outdata[i] = 1;
      continue;
    }
    else if (item == Py_False || item == Py_Zero) {
      outdata[i] = 0;
      continue;
    }
    from = i;
    return false;
  }
  return true;
}


template <typename T>
static bool parse_as_int(PyObject* list, MemoryBuffer* membuf, int64_t& from)
{
  int64_t nrows = Py_SIZE(list);
  membuf->resize(static_cast<size_t>(nrows) * sizeof(T));
  T* outdata = static_cast<T*>(membuf->get());
  constexpr T na = GETNA<T>();
  constexpr int64_t max = static_cast<int64_t>(std::numeric_limits<T>::max());
  constexpr int64_t min = -max;

  int overflow;
  for (int j = 0; j < 2; ++j) {
    int64_t ifrom = j ? 0 : from;
    int64_t ito   = j ? from : nrows;
    for (int64_t i = ifrom; i < ito; ++i) {
      PyObject* item = PyList_GET_ITEM(list, i);
      PyTypeObject *itemtype = Py_TYPE(item);     // borrowed ref

      if (item == Py_None) {
        outdata[i] = na;
        continue;
      }
      else if (itemtype == &PyLong_Type || PyLong_Check(item)) {
        int64_t val = PyLong_AsInt64AndOverflow(item, &overflow);
        if (val <= max && val >= min && !overflow) {
          outdata[i] = static_cast<T>(val);
          continue;
        }
      }
      from = i;
      return false;
    }
  }
  return true;
}


// We don't attempt to parse as float because Python internally stores numbers
// as doubles, and it's extremely hard to determine whether that number should
// have been a float instead...
static bool parse_as_double(PyObject* list, MemoryBuffer* membuf, int64_t& from)
{
  int64_t nrows = Py_SIZE(list);
  membuf->resize(static_cast<size_t>(nrows) * sizeof(double));
  double* outdata = static_cast<double*>(membuf->get());
  constexpr double na = GETNA<double>();
  constexpr double inf = std::numeric_limits<double>::infinity();
  // Largest integer that can still be represented as double without loss of
  // precision. Thus, (2^53 + 1) is the first integer that cannot be stored in
  // a double and retrieved back: `(long)((double)(maxlong + 1)) != maxlong`.
  constexpr double maxlong = 9007199254740992;  // = 2^53

  for (int j = 0; j < 2; ++j) {
    int64_t ifrom = j ? 0 : from;
    int64_t ito   = j ? from : nrows;
    for (int64_t i = ifrom; i < ito; ++i) {
      PyObject* item = PyList_GET_ITEM(list, i);
      PyTypeObject *itemtype = Py_TYPE(item);     // borrowed ref

      if (item == Py_None) {
        outdata[i] = na;
        continue;
      }
      else if (itemtype == &PyLong_Type || PyLong_Check(item)) {
        double val = PyLong_AsDouble(item);
        if (val == -1 && PyErr_Occurred()) {
          PyErr_Clear();
          // TODO: only do this under "force" mode
          int sign = _PyLong_Sign(item);
          outdata[i] = sign > 0? inf : -inf;
          continue;
        }
        // else if (val <= maxlong && val >= -maxlong) {
          outdata[i] = val;
          continue;
        // }
      }
      else if (itemtype == &PyFloat_Type || PyFloat_Check(item)) {
        outdata[i] = PyFloat_AS_DOUBLE(item);
        continue;
      }
      from = i;
      return false;
    }
  }
  return true;
}



template <typename T>
static bool parse_as_str(PyObject* list, MemoryBuffer* offbuf,
                         MemoryBuffer*& strbuf)
{
  int64_t nrows = Py_SIZE(list);
  offbuf->resize(static_cast<size_t>(nrows + 1) * sizeof(T));
  T* offsets = static_cast<T*>(offbuf->get()) + 1;
  offsets[-1] = -1;
  if (strbuf == nullptr) {
    strbuf = new MemoryMemBuf(static_cast<size_t>(nrows) * 4);
  }

  T curr_offset = 1;
  int64_t i;
  for (i = 0; i < nrows; ++i) {
    PyObject* item = PyList_GET_ITEM(list, i);
    PyTypeObject *itemtype = Py_TYPE(item);

    if (item == Py_None) {
      offsets[i] = -curr_offset;
      continue;
    }
    if (itemtype == &PyUnicode_Type) {
      PyObject *pybytes = PyUnicode_AsEncodedString(item, "utf-8", "strict");
      if (!pybytes) break;
      const char* cstr = PyBytes_AsString(pybytes);
      const Py_ssize_t len = Py_SIZE(pybytes);
      if (len) {
        T tlen = static_cast<T>(len);
        T next_offset = curr_offset + tlen;
        size_t next_size = static_cast<size_t>(next_offset);
        if (static_cast<Py_ssize_t>(tlen) != len || next_offset < curr_offset) {
          break;
        }
        if (strbuf->size() < next_size) {
          strbuf->resize(next_size);
        }
        memcpy(strbuf->at(curr_offset - 1), cstr, static_cast<size_t>(len));
        curr_offset = next_offset;
      }
      offsets[i] = curr_offset;
      Py_DECREF(pybytes);
      continue;
    }
    break;
  }
  if (i < nrows) {
    if (sizeof(T) == sizeof(int64_t)) {
      strbuf->release();
      strbuf = nullptr;
    }
    return false;
  } else {
    strbuf->resize(static_cast<size_t>(curr_offset - 1));
    return true;
  }
}


static bool parse_as_pyobj(PyObject* list, MemoryBuffer* membuf)
{
  int64_t nrows = Py_SIZE(list);
  membuf->resize(static_cast<size_t>(nrows) * sizeof(PyObject*));
  PyObject** outdata = static_cast<PyObject**>(membuf->get());

  for (int64_t i = 0; i < nrows; ++i) {
    PyObject* item = PyList_GET_ITEM(list, i);
    outdata[i] = item;
    Py_INCREF(item);
  }
  return true;
}


static int find_next_stype(int curr_stype, int stype0, int ltype0) {
  if (stype0) {
    return curr_stype == 0? stype0 : 0;
  }
  if (ltype0) {
    for (int i = curr_stype + 1; i < DT_STYPES_COUNT; i++) {
      if (stype_info[i].ltype == ltype0) return i;
    }
    return 0;
  }
  return (curr_stype + 1) % DT_STYPES_COUNT;
}



Column* Column::from_pylist(PyObject* list, int stype0, int ltype0)
{
  if (stype0 && ltype0) throw new Error("Cannot fix both stype and ltype");
  if (list == NULL || !PyList_Check(list))
    throw new Error("Python list is expected");

  MemoryBuffer* membuf = new MemoryMemBuf(0);
  MemoryBuffer* strbuf = nullptr;
  int stype = find_next_stype(0, stype0, ltype0);
  int64_t i = 0;
  while (stype) {
    bool ret = false;
    switch (stype) {
      case ST_BOOLEAN_I1: ret = parse_as_bool(list, membuf, i); break;
      case ST_INTEGER_I1: ret = parse_as_int<int8_t>(list, membuf, i); break;
      case ST_INTEGER_I2: ret = parse_as_int<int16_t>(list, membuf, i); break;
      case ST_INTEGER_I4: ret = parse_as_int<int32_t>(list, membuf, i); break;
      case ST_INTEGER_I8: ret = parse_as_int<int64_t>(list, membuf, i); break;
      case ST_REAL_F8:    ret = parse_as_double(list, membuf, i); break;
      case ST_STRING_I4_VCHAR:
        ret = parse_as_str<int32_t>(list, membuf, strbuf);
        break;
      case ST_STRING_I8_VCHAR:
        ret = parse_as_str<int64_t>(list, membuf, strbuf);
        break;
      case ST_OBJECT_PYPTR:
        ret = parse_as_pyobj(list, membuf);
        break;
      default:
        ; /* do nothing -- not all STypes are currently implemented. */
    }
    if (ret) {
      Column* col = Column::new_column(static_cast<SType>(stype));
      col->replace_buffer(membuf, strbuf);
      if (membuf) membuf->release();
      if (strbuf) strbuf->release();
      return col;
    }
    stype = find_next_stype(stype, stype0, ltype0);
  }
  // Shouldn't be reachable...
  throw new Error("Unable to create a Column from this list");
}
