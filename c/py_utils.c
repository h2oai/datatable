#include <exception>
#include <stdexcept>
#include <string>
#include <vector>
#include <string.h>  // memcpy
#include "py_datatable.h"
#include "py_utils.h"


/**
 * Create and return a new instance of python's None object
 * (actually, this reuses Py_None singleton, increasing its REFCNT)
 */
PyObject* none(void) {
    Py_INCREF(Py_None);
    return Py_None;
}


/**
 * Increment reference count of PyObject `x`, and then return it.
 */
PyObject* incref(PyObject *x) {
    Py_XINCREF(x);
    return x;
}


/**
 * Decrement reference count of PyObject `x`, and then return NULL.
 */
PyObject* decref(PyObject *x) {
    Py_XDECREF(x);
    return NULL;
}


void* _dt_malloc(size_t n)
{
    if (!n) return NULL;
    void *res = malloc(n);
    if (res == NULL) {
        PyErr_Format(PyExc_MemoryError, "Failed to allocate %zd bytes", n);
    }
    return res;
}


void* _dt_realloc(void *ptr, size_t n)
{
    if (!n) return NULL;
    void *res = realloc(ptr, n);
    if (res == NULL) {
        PyErr_Format(PyExc_MemoryError, "Failed to allocate %zd bytes", n);
    }
    return res;
}


void* _dt_calloc(size_t n, size_t size)
{
    if (!n) return NULL;
    void *res = calloc(n, size);
    if (res == NULL) {
        PyErr_Format(PyExc_MemoryError, "Failed to allocate %zd bytes",
                     n * size);
    }
    return res;
}


void _dt_free(void *ptr)
{
    free(ptr);
}


__attribute__ ((format(printf, 1, 2)))
void _dt_err_r(const char *format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    PyErr_FormatV(PyExc_RuntimeError, format, vargs);
    va_end(vargs);
}


__attribute__ ((format(printf, 1, 2)))
void _dt_err_v(const char *format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    PyErr_FormatV(PyExc_ValueError, format, vargs);
    va_end(vargs);
}


__attribute__ ((format(printf, 1, 2)))
void _dt_err_a(const char *format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    PyErr_FormatV(PyExc_AssertionError, format, vargs);
    va_end(vargs);
}


char* _to_string(PyObject *x, PyObject **tmp)
{
    if (x == NULL) goto fail;
    if (x == Py_None) {
        Py_DECREF(x);
        return NULL;
    } else {
        PyObject *z = NULL;
        if (PyUnicode_Check(x)) {
            z = PyUnicode_AsEncodedString(x, "utf-8", "strict");
            Py_DECREF(x);
        } else if (PyBytes_Check(x)) {
            z = x;
        }
        if (z == NULL) goto fail;
        char *out = NULL;
        if (tmp == NULL) {
            dtmalloc_g(out, char, PyBytes_Size(z) + 1);
            memcpy(out, PyBytes_AsString(z), (size_t)PyBytes_Size(z) + 1);
        } else {
            *tmp = z;
            out = PyBytes_AsString(z);
        }
        return out;
    }
  fail:
    return (char*) -1;
}

char** _to_string_list(PyObject *x) {
    if (x == NULL) return (char**) -1;
    char **res = NULL;
    if (x == Py_None) {}
    else if (PyList_Check(x) || PyTuple_Check(x)) {
        int islist = PyList_Check(x);
        Py_ssize_t count = islist? PyList_Size(x) : PyTuple_Size(x);
        dtcalloc(res, char*, count + 1);
        for (Py_ssize_t i = 0; i < count; i++) {
            PyObject *item = islist? PyList_GetItem(x, i) : PyTuple_GetItem(x, i);
            if (PyUnicode_Check(item)) {
                PyObject *y = PyUnicode_AsEncodedString(item, "utf-8", "strict");
                if (y == NULL) goto fail;
                size_t len = (size_t) PyBytes_Size(y);
                dtmalloc_g(res[i], char, len + 1);
                memcpy(res[i], PyBytes_AsString(y), len + 1);
                Py_DECREF(y);
            } else
            if (PyBytes_Check(item)) {
                size_t len = (size_t) PyBytes_Size(x);
                dtmalloc_g(res[i], char, len + 1);
                memcpy(res[i], PyBytes_AsString(x), len + 1);
            } else {
                PyErr_Format(PyExc_TypeError,
                    "Argument %d in the list is not a string: %R (%R)",
                    i, item, PyObject_Type(item));
                goto fail;
            }
        }
    } else {
        PyErr_Format(PyExc_TypeError,
            "A list of strings is expected, got %R", x);
        goto fail;
    }
    Py_DECREF(x);
    return res;
  fail:
    return (char**) -1;
}


/**
 * Retrieve the value of `pyobj.attr` as a boolean. If that value is None, then
 * `res` is returned (which is normally false). If the value is not boolean,
 * an exception will be raised.
 */
bool get_attr_bool(PyObject *pyobj, const char *attr, bool res)
{
  PyObject *x = PyObject_GetAttrString(pyobj, attr);
  if (!x) throw PyError();
  if (x == Py_True) res = true;
  else if (x == Py_False) res = false;
  else if (x != Py_None) {
    Py_DECREF(x);
    throw ValueError() << "Attribute `" << attr << "` is not boolean";
  }
  Py_DECREF(x);
  return res;
}


/**
 * Retrieve the value of `pyobj.attr` as an int64. If the value is None in
 * Python, then the default `res` is returned. This function will throw an
 * exception if the object does not have the specified attribute, or if the
 * retrieved value is not integer.
 */
int64_t get_attr_int64(PyObject *pyobj, const char *attr, int64_t res)
{
  PyObject *x = PyObject_GetAttrString(pyobj, attr);
  if (!x) throw PyError();
  if (PyLong_Check(x)) {
    res = static_cast<int64_t>(PyLong_AsLongLong(x));
  } else if (x != Py_None) {
    Py_DECREF(x);
    throw ValueError() << "Attribute `" << attr << "` is not integer";
  }
  Py_DECREF(x);
  return res;
}


/**
 * Retrieve the value of `pyobj.attr` as a string. If the value is None in
 * Python, then an empty string is returned. Otherwise, the function converts
 * Python string/bytes object into the C++ std::string and returns it.
 *
 * The string is returned by-value, which is supposed to be optimized by the
 * compiler (http://en.cppreference.com/w/cpp/language/copy_elision):
 *
 *     If a function returns a class type by value, and the return statement's
 *     expression is the name of a non-volatile object with automatic storage
 *     duration, which isn't the function parameter, or a catch clause
 *     parameter, and which has the same type (ignoring top-level
 *     cv-qualification) as the return type of the function, then copy/move
 *     (since C++11) is omitted. When that local object is constructed, it is
 *     constructed directly in the storage where the function's return value
 *     would otherwise be moved or copied to. This variant of copy elision is
 *     known as NRVO, "named return value optimization".
 */
std::string get_attr_string(PyObject *pyobj, const char *attr)
{
  PyObject *x = PyObject_GetAttrString(pyobj, attr);
  PyObject *z = NULL;
  if (!x) throw PyError();
  if (PyUnicode_Check(x)) {
    z = PyUnicode_AsEncodedString(x, "utf-8", "strict");
  } else if (PyBytes_Check(x)) {
    z = x;
    Py_INCREF(x);
  } else if (x == Py_None) {
  } else {
    throw ValueError() << "Attribute `" << attr << "` is not a string";
  }
  // This will copy the contents of z
  std::string res(z? PyBytes_AsString(z) : "");
  Py_XDECREF(x);
  Py_XDECREF(z);
  return res;
}


/**
 * Retrieve the value of `pyobj.attr` as a `DataTable*` object. If the value is
 * None in Python, then a null pointer is returned. If `pyobj.attr` does not
 * correspond to a DataTable PyObject then an exception is raised.
 */
DataTable* get_attr_datatable(PyObject* pyobj, const char* attr)
{
  DataTable* dt = nullptr;
  PyObject* pydt = PyObject_GetAttrString(pyobj, attr);
  int ret = dt_unwrap(pydt, &dt);
  Py_XDECREF(pydt);
  if (!ret) throw Error();
  return dt;
}


/**
 * Retrieve the value of `pyobj.attr` as a vector of strings. On Python side the
 * value can be either None (in which case nullptr is returned), or a List /
 * Tuple of strings / bytes objects. Anything else will cause an exception to
 * be raised.
 *
 * The function takes a reference to a vector of strings `res`, and modifies it
 * in-place. The lifetime of this vector is thus controlled by the calling code.
 * Usage:
 *
 *     std::vector<std::string> foo;
 *     get_attr_stringlist(pyobj, "attrfoo", foo);
 *
 */
void get_attr_stringlist(PyObject *pyobj, const char *attr,
                         std::vector<std::string>& res)
{
  PyObject *x = PyObject_GetAttrString(pyobj, attr);
  if (!x) throw PyError();

  if (x == Py_None) {}
  else if (PyList_Check(x) || PyTuple_Check(x)) {
    int islist = PyList_Check(x);
    Py_ssize_t count = islist? PyList_Size(x) : PyTuple_Size(x);
    res.reserve(static_cast<size_t>(count));

    for (Py_ssize_t i = 0; i < count; i++) {
      PyObject *item = islist? PyList_GetItem(x, i) : PyTuple_GetItem(x, i);
      if (PyUnicode_Check(item)) {
        PyObject *y = PyUnicode_AsEncodedString(item, "utf-8", "strict");
        if (!y) throw PyError();
        res.push_back(PyBytes_AsString(y));
        Py_DECREF(y);
      } else if (PyBytes_Check(item)) {
        res.push_back(PyBytes_AsString(item));
      } else {
        throw TypeError() << "Argument " << i << " in the list is not a string"
                          << ": " << item << " (" << PyObject_Type(item) << ")";
      }
    }
  } else {
    throw TypeError() << "A list of strings is expected, got " << x;
  }
  Py_XDECREF(x);
}
