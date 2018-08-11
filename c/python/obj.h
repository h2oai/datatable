//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_OBJ_h
#define dt_PYTHON_OBJ_h
#include <string>
#include <vector>
#include <Python.h>
#include "types.h"             // CString
#include "utils/exceptions.h"  // Error

class Column;
class DataTable;
class Groupby;
class RowIndex;
class PyyLong;   // TODO: remove
class PyyFloat;  // TODO: remove

namespace py {
class list;
class obj;
class oobj;
using strvec = std::vector<std::string>;


class _obj {
  protected:
    PyObject* v;
    struct error_manager;  // see below

  public:
    oobj get_attr(const char* attr) const;
    oobj invoke(const char* fn, const char* format, ...) const;

    PyyFloat __float__() const;
    oobj     __str__() const;
    int8_t   __bool__() const;

    bool is_none() const;
    bool is_ellipsis() const;
    bool is_true() const;
    bool is_false() const;
    bool is_bool() const;
    bool is_int() const;
    bool is_float() const;
    bool is_numeric() const;
    bool is_string() const;
    bool is_list() const;
    bool is_tuple() const;
    bool is_dict() const;
    bool is_buffer() const;

    int8_t   to_bool          (const error_manager& = _em0) const;
    int8_t   to_bool_strict   (const error_manager& = _em0) const;
    int8_t   to_bool_force    (const error_manager& = _em0) const;
    int32_t  to_int32         (const error_manager& = _em0) const;
    int32_t  to_int32_strict  (const error_manager& = _em0) const;
    int32_t  to_int32_truncate(const error_manager& = _em0) const;
    int32_t  to_int32_mask    (const error_manager& = _em0) const;
    int64_t  to_int64         (const error_manager& = _em0) const;
    int64_t  to_int64_strict  (const error_manager& = _em0) const;
    double   to_double        (const error_manager& = _em0) const;
    CString     to_cstring    (const error_manager& = _em0) const;
    std::string to_string     (const error_manager& = _em0) const;
    PyObject*   to_pyobject_newref() const;
    py::list    to_list       (const error_manager& = _em0) const;
    PyyLong     to_pyint      () const;
    PyyLong     to_pyint_force() const;
    PyyFloat    to_pyfloat    () const;
    char**      to_cstringlist() const;
    strvec      to_stringlist () const;

    Column*     to_column     (const error_manager& = _em0) const;
    Groupby*    to_groupby    (const error_manager& = _em0) const;
    RowIndex    to_rowindex   (const error_manager& = _em0) const;
    DataTable*  to_frame      (const error_manager& = _em0) const;

  protected:
    /**
     * `error_manager` is a factory function for different error messages. It
     * is used to customize error messages when they are thrown from an `Arg`
     * instance.
     */
    struct error_manager {
      virtual ~error_manager() {}
      virtual Error error_not_boolean(PyObject*) const;
      virtual Error error_not_integer(PyObject*) const;
      virtual Error error_not_double(PyObject*) const;
      virtual Error error_not_string(PyObject*) const;
      virtual Error error_not_groupby(PyObject*) const;
      virtual Error error_not_rowindex(PyObject*) const;
      virtual Error error_not_frame(PyObject*) const;
      virtual Error error_not_column(PyObject*) const;
      virtual Error error_not_list(PyObject*) const;
      virtual Error error_int32_overflow(PyObject*) const;
      virtual Error error_int64_overflow(PyObject*) const;
    };
    static error_manager _em0;

    // `_obj` class is not directly constructible: create either `obj` or
    // `oobj` objects instead.
    _obj() = default;

  private:
    template <int MODE>
    int32_t _to_int32(const error_manager& em) const;

    friend oobj;
};


class obj : public _obj {
  public:
    obj(PyObject* p);
    obj(const obj&);
    obj& operator=(const obj&);

    PyObject* to_borrowed_ref() const { return v; }
};


class oobj : public _obj {
  public:
    oobj();
    oobj(PyObject* p);
    oobj(const oobj&);
    oobj(const obj&);
    oobj(oobj&&);
    oobj& operator=(const oobj&);
    oobj& operator=(oobj&&);
    ~oobj();

    static oobj from_new_reference(PyObject* p);
    PyObject* release();

};


/**
 * Return python constants None, True, False wrapped as `oobj`s.
 */
oobj None();
oobj True();
oobj False();


}  // namespace py

#endif
