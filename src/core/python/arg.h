//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#ifndef dt_PYTHON_ARG_h
#define dt_PYTHON_ARG_h
#include <string>     // std::string
#include <vector>     // std::vector
#include "python/_all.h"
#include "python/list.h"
namespace py {

class ArgParent;

/**
 * The argument may be in "undefined" state, meaning the user did not provide
 * a value for this argument in the function/method call. This state can be
 * checked with the `is_undefined()` method.
 */
class Arg : public _obj::error_manager {
  private:
    size_t pos;
    ArgParent* parent;
    py::robj pyobj;
    mutable std::string cached_name;

  public:
    Arg();
    explicit Arg(const std::string&);
    Arg(py::_obj, const std::string&);
    Arg(const Arg&) = default;
    virtual ~Arg() override;
    void init(size_t i, ArgParent* args);
    void set(PyObject* value);

    //---- Type checks -----------------
    bool is_auto() const;  // check for string "auto"
    bool is_bool() const;
    bool is_bytes() const;
    bool is_defined() const;
    bool is_dict() const;
    bool is_ellipsis() const;
    bool is_float() const;
    bool is_frame() const;
    bool is_int() const;
    bool is_list() const;
    bool is_list_or_tuple() const;
    bool is_none() const;
    bool is_none_or_undefined() const;
    bool is_numpy_array() const;
    bool is_pandas_frame() const;
    bool is_pandas_series() const;
    bool is_range() const;
    bool is_string() const;
    bool is_tuple() const;
    bool is_undefined() const;

    //---- Type conversions ------------
    bool        to_bool_strict        () const;
    dt::CString to_cstring            () const;
    int32_t     to_int32_strict       () const;
    int64_t     to_int64_strict       () const;
    size_t      to_size_t             () const;
    double      to_double             () const;
    py::olist   to_pylist             () const;
    py::odict   to_pydict             () const;
    py::rdict   to_rdict              () const;
    py::otuple  to_otuple             () const;
    std::string to_string             () const;
    strvec      to_stringlist         () const;
    dt::SType   to_stype              () const;
    dt::SType   to_stype              (const error_manager&) const;
    py::oobj    to_oobj               () const { return oobj(pyobj); }
    py::oobj    to_oobj_or_none       () const { return pyobj? oobj(pyobj) : py::None(); }
    py::robj    to_robj               () const { return pyobj; }
    py::oiter   to_oiter              () const;
    DataTable*  to_datatable          () const;


    //---- Error messages --------------
    virtual Error error_not_list           (PyObject*) const override;
    virtual Error error_not_stype          (PyObject*) const override;
    virtual Error error_not_boolean        (PyObject*) const override;
    virtual Error error_not_integer        (PyObject*) const override;
    virtual Error error_int_negative       (PyObject*) const override;
    virtual Error error_not_double         (PyObject*) const override;
    virtual Error error_not_string         (PyObject*) const override;
    virtual Error error_not_iterable       (PyObject*) const override;

    // ?
    explicit operator bool() const noexcept { return pyobj.operator bool(); }
    PyObject* robj() const { return pyobj.to_pyobject_newref(); }
    PyObject* to_borrowed_ref() const { return pyobj.to_borrowed_ref(); }
    PyTypeObject* typeobj() const { return pyobj.typeobj(); }
    void print() const;

    operator int32_t() const;
    operator int64_t() const;
    operator size_t() const;
    operator dt::SType() const;

    // This template is specialized for different types below
    template <typename T> T to(const T& deflt) const;

    /**
     * Convert argument to different list objects.
     */
    // operator list() const;
    // std::vector<std::string> to_list_of_strs() const;

    const std::string& name() const;
    const char* short_name() const;

  private:
    void _check_list_or_tuple() const;
    void _check_missing() const;
};



template <>
inline bool Arg::to(const bool& deflt) const {
  return is_none_or_undefined()? deflt : to_bool_strict();
}

template <>
inline int32_t Arg::to(const int32_t& deflt) const {
  return is_none_or_undefined()? deflt : to_int32_strict();
}

template <>
inline int64_t Arg::to(const int64_t& deflt) const {
  return is_none_or_undefined()? deflt : to_int64_strict();
}

template <>
inline size_t Arg::to(const size_t& deflt) const {
    return is_none_or_undefined()? deflt : to_size_t();
}

template <>
inline double Arg::to(const double& deflt) const {
  return is_none_or_undefined()? deflt : to_double();
}

template <>
inline oobj Arg::to(const oobj& deflt) const {
  return is_none_or_undefined()? deflt : to_oobj();
}

template <>
inline std::string Arg::to(const std::string& deflt) const {
  return is_none_or_undefined()? deflt : to_string();
}

template <>
inline strvec Arg::to(const strvec& deflt) const {
  return is_none_or_undefined()? deflt : to_stringlist();
}


}  // namespace py
#endif
