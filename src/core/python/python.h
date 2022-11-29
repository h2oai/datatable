//------------------------------------------------------------------------------
// Copyright 2018-2022 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_h
#define dt_PYTHON_h

// `locale` must be included before <Python.h>, otherwise datatable won't
// compile on macOS with Apple LLVM clang 9.1.0
#include <locale>
#include <Python.h>


// The following code has been adopted from python/pythoncapi-compat, see
// https://github.com/python/pythoncapi-compat/blob/main/pythoncapi_compat.h
//
// As per bpo-39573, `Py_SET_REFCNT()` and `Py_SET_TYPE()` are only available
// as of python 3.9.0a4, so for older versions we have to directly change
// the corresponding `ob_*` properties.
#if PY_VERSION_HEX < 0x030900A4
  static inline void _Py_SET_REFCNT(PyObject *ob, Py_ssize_t refcnt) {
    ob->ob_refcnt = refcnt;
  }
  #define Py_SET_REFCNT(ob, refcnt) _Py_SET_REFCNT(_PyObject_CAST(ob), refcnt)

  static inline void _Py_SET_TYPE(PyObject *ob, PyTypeObject *type) {
    ob->ob_type = type;
  }
  #define Py_SET_TYPE(ob, type) _Py_SET_TYPE(_PyObject_CAST(ob), type)
#endif

#endif

