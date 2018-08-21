//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_ARG_h
#define dt_PYTHON_ARG_h
#include <string>     // std::string
#include <vector>     // std::vector
#include <Python.h>
#include "python/obj.h"
#include "python/list.h"

namespace py {

class PKArgs;


/**
 * The argument may be in "undefined" state, meaning the user did not provide
 * a value for this argument in the function/method call. This state can be
 * checked with the `is_undefined()` method.
 */
class Arg : public _obj::error_manager {
  private:
    size_t pos;
    PKArgs* parent;
    py::obj pyobj;
    mutable std::string cached_name;

  public:
    Arg();
    Arg(const Arg&) = default;
    virtual ~Arg();
    void init(size_t i, PKArgs* args);
    void set(PyObject* value);

    //---- Type checks -----------------
    bool is_undefined() const;
    bool is_none() const;
    bool is_ellipsis() const;
    bool is_int() const;
    bool is_float() const;
    bool is_list() const;
    bool is_tuple() const;
    bool is_list_or_tuple() const;
    bool is_dict() const;
    bool is_string() const;
    bool is_range() const;

    //---- Type conversions ------------
    py::list    to_pylist        () const;


    //---- Error messages --------------
    virtual Error error_not_list       (PyObject*) const;

    // ?
    PyObject* obj() { return pyobj.to_pyobject_newref(); }
    void print() const;

    /**
     * Convert argument to int32/int64. An exception will be thrown if the
     * argument is None, or not of integer type, or if the integer value is
     * too large.
     * This method must not be called if the argument is undefined.
     */
    // operator int32_t() const;
    // operator int64_t() const;

    /**
     * Convert argument to different list objects.
     */
    // operator list() const;
    // std::vector<std::string> to_list_of_strs() const;

    const std::string& name() const;

  private:
    void _check_list_or_tuple() const;
    void _check_missing() const;
};


}  // namespace py
#endif
