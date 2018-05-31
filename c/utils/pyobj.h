//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_UTILS_PYOBJ_H
#define dt_UTILS_PYOBJ_H
#include <Python.h>
#include <stdint.h>
#include <string>
#include <vector>

class DataTable;
class Column;
class RowIndex;
class Groupby;
class PyyList;
class PyyLong;
class PyyFloat;
class PyyString;
extern PyObject* Py_One;
extern PyObject* Py_Zero;


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
  static PyObject* None;

public:
  PyObj();
  PyObj(PyObject* const&);
  PyObj(PyObject*&&);
  PyObj(PyObject*, const char* attr);
  PyObj(const PyObj&);
  PyObj(PyObj&&);
  PyObj& operator=(PyObj other);
  ~PyObj();
  static PyObj none();

  friend void swap(PyObj& first, PyObj& second) noexcept {
    using std::swap;
    swap(first.obj, second.obj);
    swap(first.tmp, second.tmp);
  }

  /**
   * Retrieve attribute `a` from this python object. This is equivalent to
   * `getattr(obj, a)` in Python. If the object does not have such an
   * attribute, an exception will be raised.
   */
  PyObj attr(const char* a) const;

  /**
   * Call method `fn` on this object, passing an arbitrary list of arguments.
   * If the method returns an error, an exception will be thrown.
   */
  PyObj invoke(const char* fn, const char* format, ...) const;

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
   * of scope). If the object is Py_None, a null pointer is returned. If a
   * size_t* pointer is passed, then string length will be stored in that
   * pointer (the length doesn't take into account the last \0 byte).
   *
   * The `as_ccstring` method is similar to `as_cstring`, however the returned
   * pointer will be owned by the caller, and the caller is responsible for its
   * deallocation (via delete[]). This method is also less efficient than
   * `as_cstring`, since it allocates and copies the string data into a new
   * character buffer. If the object is Py_None, a null pointer is returned.
   */
  std::string as_string() const;
  const char* as_cstring() const;
  const char* as_cstring(size_t*) const;
  char* as_ccstring() const;

  /**
   * If this PyObj is a string, return its first character. When the python
   * object is None, the value `ifnone` is returned; and if the string is empty
   * the value `ifempty`. The no-arguments form uses `ifnone = ifempty = '\0'`.
   */
  char as_char(char ifnone, char ifempty) const;
  char as_char() const;

  /**
   * Return the underlying PyObject* object, as a new reference. The caller will
   * own the returned reference.
   */
  PyObject* as_pyobject() const;

  DataTable* as_datatable() const;
  Column* as_column() const;
  Groupby* as_groupby() const;

  /**
   * Assuming the underlying object is pyrowindex::obj, return its RowIndex
   * content object. An empty RowIndex will also be returned if the underlying
   * object is None. In all other cases an exception will be thrown.
   *
   * Example:
   *   PyObject* arg1;
   *   if (!PyArg_ParseTuple("O", &arg1)) return NULL;
   *   RowIndex ri = PyObj(arg1).as_rowindex();
   */
  RowIndex as_rowindex() const;

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

  void print();
  PyObj __str__() const;

  /**
   * Cast PyObject into boolean (using python's `bool(x)`), and return 1, 0 or
   * NA depending on whether the value was truthy, falsy, or non-convertible.
   */
  int8_t __bool__() const;

  /**
   * Cast into a PyyLong, using python call `int(x)`. If the conversion fails,
   * an "empty" PyyLong object is returned.
   */
  PyyLong __int__() const;

  /**
   * Cast into a PyyFloat, using python call `float(x)`. If the conversion
   * fails, an "empty" PyyFloat object is returned.
   */
  PyyFloat __float__() const;

  bool is_none() const   { return obj == Py_None; }
  bool is_true() const   { return obj == Py_True || obj == Py_One; }
  bool is_false() const  { return obj == Py_False || obj == Py_Zero; }
  bool is_long() const   { return PyLong_Check(obj); }
  bool is_float() const  { return PyFloat_Check(obj); }
  bool is_list() const   { return PyList_Check(obj); }
  bool is_string() const { return PyUnicode_Check(obj); }

  operator PyyList() const;
  operator PyyLong() const;
  operator PyyFloat() const;
  operator PyyString() const;
};


#endif
