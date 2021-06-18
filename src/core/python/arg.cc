//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include <cstdio>
#include "cstring.h"
#include "python/arg.h"
#include "python/args.h"        // py::ArgParent
#include "python/dict.h"
#include "python/int.h"
#include "python/list.h"
#include "utils/exceptions.h"
namespace py {

std::string _nth(size_t i);



//------------------------------------------------------------------------------
// Construction / initialization
//------------------------------------------------------------------------------

Arg::Arg()
  : pos(0), parent(nullptr), pyobj(nullptr) {}

Arg::Arg(py::_obj py_object, const std::string& cached_name_)
  : pos(0),
    parent(nullptr),
    pyobj(py_object.to_borrowed_ref()),
    cached_name(cached_name_) {}

Arg::Arg(const std::string& cached_name_)
  : pos(0),
    parent(nullptr),
    pyobj(nullptr),
    cached_name(cached_name_) {}

Arg::~Arg() {}


void Arg::init(size_t i, ArgParent* args) {
  pos = i;
  parent = args;
}


void Arg::set(PyObject* value) {
  pyobj = py::robj(value);
}


const std::string& Arg::name() const {
  if (cached_name.empty()) {
    bool single_arg = (pos == 0) &&
                      parent->n_positional_args() == 1 &&
                      parent->n_keyword_args() == 0 &&
                      parent->n_positional_or_keyword_args() == 0 &&
                      !parent->has_varargs() &&
                      !parent->has_varkwds();
    if (single_arg) {
      cached_name = "The argument";
    }
    else if (pos < parent->n_positional_args()) {
      cached_name = (pos == 0)? "First" :
            (pos == 1)? "Second" :
            (pos == 2)? "Third" :
            std::to_string(pos + 1) + "th";
      cached_name += " argument";
    } else {
      cached_name = std::string("Argument `") + parent->arg_name(pos) + '`';
    }
    cached_name += std::string(" in ") + parent->descriptive_name(true);
  }
  return cached_name;
}

const char* Arg::short_name() const {
  return parent->arg_name(pos);
}



//------------------------------------------------------------------------------
// Type checks
//------------------------------------------------------------------------------

bool Arg::is_undefined()         const { return pyobj.is_undefined(); }
bool Arg::is_none()              const { return pyobj.is_none(); }
bool Arg::is_none_or_undefined() const { return pyobj.is_none() ||
                                                pyobj.is_undefined(); }
bool Arg::is_defined()           const { return !is_none_or_undefined(); }

bool Arg::is_ellipsis()          const { return pyobj.is_ellipsis(); }
bool Arg::is_bool()              const { return pyobj.is_bool(); }
bool Arg::is_bytes()             const { return pyobj.is_bytes(); }
bool Arg::is_int()               const { return pyobj.is_int(); }
bool Arg::is_float()             const { return pyobj.is_float(); }
bool Arg::is_list()              const { return pyobj.is_list(); }
bool Arg::is_tuple()             const { return pyobj.is_tuple(); }
bool Arg::is_list_or_tuple()     const { return pyobj.is_list_or_tuple(); }
bool Arg::is_dict()              const { return pyobj.is_dict(); }
bool Arg::is_string()            const { return pyobj.is_string(); }
bool Arg::is_range()             const { return pyobj.is_range(); }
bool Arg::is_frame()             const { return pyobj.is_frame(); }
bool Arg::is_pandas_frame()      const { return pyobj.is_pandas_frame(); }
bool Arg::is_pandas_series()     const { return pyobj.is_pandas_series(); }
bool Arg::is_numpy_array()       const { return pyobj.is_numpy_array(); }
bool Arg::is_arrow_table()       const { return pyobj.is_arrow_table(); }

bool Arg::is_auto() const {
  return pyobj.is_string() &&
         PyUnicode_CompareWithASCIIString(pyobj.get(), "auto") == 0;
}



//------------------------------------------------------------------------------
// Type conversions
//------------------------------------------------------------------------------

bool        Arg::to_bool_strict()  const { return pyobj.to_bool_strict(*this); }
dt::CString Arg::to_cstring()      const { return pyobj.to_cstring(*this); }
int32_t     Arg::to_int32_strict() const { return pyobj.to_int32_strict(*this); }
int64_t     Arg::to_int64_strict() const { return pyobj.to_int64_strict(*this); }
size_t      Arg::to_size_t()       const { return pyobj.to_size_t(*this); }
double      Arg::to_double()       const { return pyobj.to_double(*this); }
py::olist   Arg::to_pylist()       const { return pyobj.to_pylist(*this); }
py::odict   Arg::to_pydict()       const { return pyobj.to_pydict(*this); }
py::rdict   Arg::to_rdict()        const { return pyobj.to_rdict(*this); }
py::otuple  Arg::to_otuple()       const { return pyobj.to_otuple(*this); }
std::string Arg::to_string()       const { return pyobj.to_string(*this); }
strvec      Arg::to_stringlist()   const { return pyobj.to_stringlist(*this); }
dt::SType   Arg::to_stype()        const { return pyobj.to_stype(*this); }
dt::SType   Arg::to_stype(const error_manager& em) const { return pyobj.to_stype(em); }
dt::Type    Arg::to_type_force()   const { return pyobj.to_type_force(); }
py::oiter   Arg::to_oiter()        const { return pyobj.to_oiter(*this); }
DataTable*  Arg::to_datatable()    const { return pyobj.to_datatable(*this); }

Arg::operator int32_t() const { return to_int32_strict(); }
Arg::operator int64_t() const { return to_int64_strict(); }
Arg::operator size_t()  const { return to_size_t(); }
Arg::operator dt::SType() const { return to_stype(); }


//------------------------------------------------------------------------------
// Error messages
//------------------------------------------------------------------------------

Error Arg::error_not_list(PyObject* src) const {
  return TypeError() << name() << " should be a list or tuple, instead got "
      << Py_TYPE(src);
}

Error Arg::error_not_stype(PyObject* src) const {
  return TypeError() << name() << " cannot be converted into an stype: " << src;
}

Error Arg::error_not_boolean(PyObject* src) const {
  return TypeError() << name() << " should be a boolean, instead got "
      << Py_TYPE(src);
}

Error Arg::error_not_integer(PyObject* src) const {
  return TypeError() << name() << " should be an integer, instead got "
      << Py_TYPE(src);
}

Error Arg::error_not_string(PyObject* src) const {
  return TypeError() << name() << " should be a string, instead got "
      << Py_TYPE(src);
}

Error Arg::error_int_negative(PyObject* src) const {
  return ValueError() << name() << " cannot be negative: " << src;
}

Error Arg::error_not_double(PyObject* src) const {
  return TypeError() << name() << " should be a float, instead got "
      << Py_TYPE(src);
}

Error Arg::error_not_iterable(PyObject* src) const {
  return TypeError() << name() << " should be an iterable, instead got "
      << Py_TYPE(src);
}


//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

void Arg::print() const {
  PyObject_Print(pyobj.v, stdout, Py_PRINT_RAW);
  std::printf("\n");
}

void Arg::_check_missing() const {
  if (pyobj.is_undefined()) {
    throw TypeError() << " is missing";
  }
}

void Arg::_check_list_or_tuple() const {
  if (!(pyobj.is_list() || pyobj.is_tuple())) {
    throw TypeError() << name() << " should be a list";
  }
}

std::string _nth(size_t i) {
  return std::to_string(i) + ((i / 10) % 10 == 1 ? "th" :
                              i % 10 == 1 ? "st" :
                              i % 10 == 2 ? "nd" :
                              i % 10 == 3 ? "rd" : "th");
}


}  // namespace py
