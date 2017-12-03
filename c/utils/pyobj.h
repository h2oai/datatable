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
#ifndef dt_UTILS_PYOBJ_H
#define dt_UTILS_PYOBJ_H
#include <Python.h>
#include <stdint.h>
#include <string>
#include <vector>

class DataTable;


/**
 * C++ wrapper around the struct `PyObject`. This class ensures proper lifetime
 * of the wrapped object (i.e. it INCREFs it upon acquiral, and DECREFs in the
 * destructor). It also provides convenient methods to convert the underlying
 * value into a C++ primitive.
 */
class PyObj
{
  PyObject* obj;
  mutable PyObject* tmp;

public:
  PyObj();
  PyObj(PyObject*);
  PyObj(PyObject*, const char* attr);
  PyObj(const PyObj&);
  ~PyObj();

  /**
   * Retrieve attribute `a` from this python object. This is equivalent to
   * `getattr(obj, a)` in Python. If the object does not have such an
   * attribute, an exception will be raised.
   */
  PyObj attr(const char* a) const;

  int8_t as_bool() const;
  int32_t as_int32() const;
  int64_t as_int64() const;
  double as_double() const;

  /**
   * Convert the object to a string. The object must be of PyUnicode or
   * PyBytes type (or Py_None).
   *
   * The `as_string` method will return the wrapped object as a standard C++
   * string. This involves copying the characters into the string's internal
   * buffer. Thus, it is less efficient but safe. If the object is Py_None,
   * an empty string is returned.
   *
   * The `as_cstring` method will return pointer to the `const char*` buffer
   * containing the string. The returned pointer will not be owned by the
   * caller, and its lifetime is tied to the lifetime of the current PyObj.
   * This method can be called only once on the PyObj. It is more efficient than
   * the first one (avoids extra copy), but less safe (the user must not free
   * the pointer accidentally, and cannot use the pointer after PyObj went out
   * of scope). If the object is Py_None, a null pointer is returned.
   *
   * The `as_ccstring` method is similar to `as_cstring`, however the returned
   * pointer will be owned by the caller, and the caller is responsible for its
   * deallocation (via delete[]). This method is also less efficient than
   * `as_cstring`, since it allocates and copies the string data into a new
   * character buffer. If the object is Py_None, a null pointer is returned.
   */
  std::string as_string() const;
  const char* as_cstring() const;
  char* as_ccstring() const;

  /**
   * If this PyObj is a string, return its first character (or '\0' if the
   * object is Py_None).
   */
  char as_char() const;

  PyObject* as_pyobject() const;
  DataTable* as_datatable() const;

  /**
   * Convert the object to a list of strings. The object must be of python type
   * `List[Union[str, bytes]]` (or Py_None).
   *
   * The `as_stringlist` method returns a vector of C++ strings.
   *
   * The `as_cstringlist` returns a NULL-terminated C-style array of C-strings.
   * The caller becomes the owner of that array, and is responsible for
   * deleting it once it's no longer needed (using `delete[]` operator).
   */
  std::vector<std::string> as_stringlist() const;
  char** as_cstringlist() const;
};


#endif
