//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_OBJ_h
#define dt_PYTHON_OBJ_h
#include <Python.h>
#include "types.h"             // CString
#include "utils/exceptions.h"  // Error

namespace py {


class _obj {
  protected:
    PyObject* obj;
    struct error_manager;  // see below

  public:
    bool is_none() const;
    bool is_ellipsis() const;
    bool is_bool() const;
    bool is_int() const;
    bool is_float() const;
    bool is_numeric() const;
    bool is_string() const;
    bool is_list() const;
    bool is_tuple() const;
    bool is_dict() const;

    int32_t to_int32(const error_manager& = _em0) const;
    int64_t to_int64(const error_manager& = _em0) const;
    CString to_cstring(const error_manager& = _em0) const;
    std::string to_string(const error_manager& = _em0) const;

  protected:
    /**
     * `error_manager` is a factory function for different error messages. It
     * is used to customize error messages when they are thrown from an `Arg`
     * instance.
     */
    struct error_manager {
      virtual ~error_manager() {}
      virtual Error error_not_integer(PyObject*) const;
      virtual Error error_not_string(PyObject*) const;
      virtual Error error_int32_overflow(PyObject*) const;
      virtual Error error_int64_overflow(PyObject*) const;
    };
    static error_manager _em0;

    // `_obj` class is not directly constructible: create either `bobj` or
    // `oobj` objects instead.
    _obj() = default;
};


class bobj : public _obj {
  public:
    bobj(PyObject* p);
    bobj(const bobj&);
    bobj& operator=(const bobj&);
};


class oobj : public _obj {
  public:
    oobj(PyObject* p);
    oobj(const oobj&);
    oobj(oobj&&);
    ~oobj();
};


}  // namespace py

#endif
