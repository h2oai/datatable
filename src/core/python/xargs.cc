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
#include <algorithm>             // std::min
#include <cstring>               // std::strcmp
#include "call_logger.h"
#include "python/xargs.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
namespace py {


//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------

XArgs::XArgs()
  : pyfn_(nullptr),
    docstring_(nullptr),
    classId_(0),
    nargs_required_(0),
    nargs_posonly_(0),
    nargs_pos_kwd_(0),
    nargs_kwdonly_(0),
    nargs_all_(0),
    has_varargs_(false),
    has_varkwds_(false),
    has_renamed_args_(false)
{
  store().push_back(this);
}

XArgs::XArgs(impl_function_t fn) : XArgs() {
  ccfn_.fn = fn;
}

XArgs::XArgs(impl_method_t method, size_t classId) : XArgs() {
  ccfn_.meth = method;
  classId_ = classId;
}

XArgs::XArgs(impl_methodv_t method, size_t classId) : XArgs() {
  ccfn_.methv = method;
  classId_ = classId;
}

std::vector<XArgs*>& XArgs::store() {
  static std::vector<XArgs*> xargs_repo;
  return xargs_repo;
}


PyMethodDef XArgs::get_method_def() {
  finish_initialization();
  return PyMethodDef {
    function_name_.data(),
    reinterpret_cast<PyCFunction>(pyfn_),
    METH_VARARGS | METH_KEYWORDS,
    docstring_
  };
}

PyCFunctionWithKeywords XArgs::get_pyfunction() {
  finish_initialization();
  return pyfn_;
}

const char* XArgs::get_docstring() const {
  return docstring_;
}


void XArgs::finish_initialization() {
  nargs_all_ = nargs_posonly_ + nargs_pos_kwd_ + nargs_kwdonly_;
  bound_args_.resize(nargs_all_);
  for (size_t i = 0; i < nargs_all_; ++i) {
    bound_args_[i].init(i, this);
  }

  xassert(arg_names_.size() == nargs_all_);
  xassert(nargs_required_ <= nargs_all_);
  xassert(ccfn_.fn);
  xassert(pyfn_);
  xassert(!function_name_.empty());
  xassert(function_name_.find('.') == std::string::npos);
  if (has_varargs_) xassert(nargs_pos_kwd_ == 0);
}



//------------------------------------------------------------------------------
// Properties
//------------------------------------------------------------------------------

XArgs* XArgs::pyfunction(PyCFunctionWithKeywords f) {
  pyfn_ = f;
  return this;
}

XArgs* XArgs::name(const char* name) {
  function_name_ = name;
  return this;
}

XArgs* XArgs::arg_names(std::initializer_list<const char*> names) {
  arg_names_ = std::vector<const char*>(names);
  return this;
}

XArgs* XArgs::n_required_args(size_t n) {
  nargs_required_ = n;
  return this;
}

XArgs* XArgs::n_positional_args(size_t n) {
  nargs_posonly_ = n;
  return this;
}

XArgs* XArgs::n_positional_or_keyword_args(size_t n) {
  nargs_pos_kwd_ = n;
  return this;
}

XArgs* XArgs::n_keyword_args(size_t n) {
  nargs_kwdonly_ = n;
  return this;
}

XArgs* XArgs::allow_varargs() {
  has_varargs_ = true;
  return this;
}

XArgs* XArgs::allow_varkwds() {
  has_varkwds_ = true;
  return this;
}

XArgs* XArgs::docs(const char* str) {
  docstring_ = str;
  return this;
}

XArgs* XArgs::add_info(int info) {
  info_ = info;
  return this;
}

XArgs* XArgs::add_synonym_arg(const char* new_name, const char* old_name) {
  constexpr size_t NPOS = size_t(-1);
  has_renamed_args_ = true;
  size_t iold = NPOS;
  size_t inew = NPOS;
  for (size_t i = 0; i < arg_names_.size(); ++i) {
    const char* name = arg_names_[i];
    if (std::strcmp(name, old_name) == 0) iold = i;
    if (std::strcmp(name, new_name) == 0) inew = i;
  }
  xassert(iold != NPOS);  // make sure that `old_name` exists
  xassert(inew == NPOS);  (void)inew;
  PyObject* py_new_name = PyUnicode_FromString(new_name);
  xassert(py_new_name);
  kwd_map_[py_new_name] = iold;
  return this;
}

XArgs* XArgs::set_class_name(const char* className) {
  class_name_ = className;
  return this;
}


size_t XArgs::n_positional_args() const {
  return nargs_posonly_;
}

size_t XArgs::n_positional_or_keyword_args() const {
  return nargs_pos_kwd_;
}

size_t XArgs::n_keyword_args() const {
  return nargs_kwdonly_;
}

bool XArgs::has_varargs() const {
  return has_varargs_;
}

bool XArgs::has_varkwds() const {
  return has_varkwds_;
}

const char* XArgs::arg_name(size_t i) const {
  xassert(i < arg_names_.size());
  return arg_names_[i];
}

int XArgs::get_info() const {
  return info_;
}



//------------------------------------------------------------------------------
// Names
//------------------------------------------------------------------------------

const std::string& XArgs::proper_name() const {
  return function_name_;
}

std::string XArgs::qualified_name() const {
  std::string out;
  if (class_name_.empty()) {
    out += "datatable.";
  } else {
    out += class_name_;
    out += '.';
  }
  out += function_name_;
  return out;
}

std::string XArgs::descriptive_name(bool lowercase) const {
  if (function_name_ == "__init__") {
    return "`" + class_name_ + "()` constructor";
  }
  std::string out = lowercase? (class_name_.empty()? "function" : "method")
                             : (class_name_.empty()? "Function" : "Method");
  out += " `";
  out += qualified_name();
  out += "()`";
  return out;
}




//------------------------------------------------------------------------------
// Evaluation
//------------------------------------------------------------------------------

// `i` is the index of the first required argument that wasn't
// provided by the user
//
Error XArgs::error_too_few_args(size_t i) const {
  xassert(i < nargs_required_);
  auto err = TypeError();
  if (i >= nargs_posonly_) {
    err << "Argument `" << arg_names_[i] << "` in " << descriptive_name(true)
        << " is required";
  }
  else {
    bool exact = (nargs_required_ >= nargs_posonly_) && (nargs_pos_kwd_ == 0);
    size_t nreq = std::min(nargs_required_, nargs_posonly_);
    err << descriptive_name() << " requires "
        << (exact? "exactly " : "at least ")
        << nreq << " positional argument" << (nreq==1? "" : "s")
        << ", but ";
    if (i == 0) err << "none were given";
    else if (i == 1) err << "only 1 was given";
    else {
      err << "only " << i << " were given";
    }
  }
  return err;
}


Error XArgs::error_too_many_args(size_t nargs) const {
  size_t max_args = nargs_posonly_ + nargs_pos_kwd_;
  auto err = TypeError();
  err << descriptive_name();
  if (max_args == 0) {
    err << " takes no positional arguments";
  } else if (max_args == 1) {
    err << " takes only one positional argument";
  } else {
    err << " takes at most " << max_args << " positional arguments";
  }
  err << ", but " << nargs << (nargs == 1? " was given" : " were given");
  return err;
}


size_t XArgs::find_kwd(PyObject* kwd) {
  auto item = kwd_map_.find(kwd);
  if (item != kwd_map_.end()) {
    return item->second;
  }
  size_t i = 0;
  for (const char* name : arg_names_) {
    if (PyUnicode_CompareWithASCIIString(kwd, name) == 0) {
      // We store the reference to `kwd` and increase its refcount, making
      // `kwd` effectively immortal. Usually this shouldn't matter -- strings
      // that are used as keyword are normally static in the program. However
      // in the extremely rare cases when the keywords are dynamic, we don't
      // want a PyObject* to be gc-ed, then re-created with the content of
      // another string while `kwd_map_` would still map it to the original
      // index...
      Py_INCREF(kwd);
      kwd_map_[kwd] = i;
      return i;
    }
    ++i;
  }
  if (has_renamed_args_) {
    for (const auto& pair : kwd_map_) {
      if (PyUnicode_Compare(kwd, pair.first) == 0) {
        Py_INCREF(kwd);
        kwd_map_[kwd] = pair.second;
        return pair.second;
      }
    }
  }
  return size_t(-1);
}



void XArgs::bind(PyObject* args, PyObject* kwds) {
  size_t n_passed_positional_args = args? static_cast<size_t>(Py_SIZE(args)) : 0;
  size_t max_positional_args = nargs_posonly_ + nargs_pos_kwd_;
  size_t n_bound_to_params_args =
      std::min(n_passed_positional_args, max_positional_args);
  // All positional args that were not bound to params are considered varargs
  n_varargs_ = n_passed_positional_args - n_bound_to_params_args;
  if (n_varargs_ && !has_varargs_) {
    throw error_too_many_args(n_passed_positional_args);
  }

  size_t i = 0;
  while (i < n_bound_to_params_args) {
    PyObject* item = PyTuple_GET_ITEM(args, i);
    bound_args_[static_cast<size_t>(i++)].set(item);
  }
  while (i < nargs_all_) {
    bound_args_[static_cast<size_t>(i++)].set(nullptr);
  }

  n_varkwds_ = 0;
  if (kwds) {
    PyObject* key, *value;
    Py_ssize_t pos = 0;
    // Iterate over all keywords args passed by the user
    while (PyDict_Next(kwds, &pos, &key, &value)) {
      size_t ikey = find_kwd(key);
      if (ikey == size_t(-1)) {
        n_varkwds_++;
        if (has_varkwds_) continue;
        throw TypeError() << descriptive_name() << " got an unexpected keyword "
          "argument `" << PyUnicode_AsUTF8(key) << "`";
      }
      if (ikey < n_bound_to_params_args) {
        throw TypeError() << descriptive_name() << " got multiple values for "
          "argument `" << PyUnicode_AsUTF8(key) << "`";
      }
      if (ikey < nargs_posonly_) {
        throw TypeError() << descriptive_name() << " got argument `"
          << PyUnicode_AsUTF8(key) << "` as a keyword, but it should be "
            "positional-only";
      }
      bound_args_[ikey].set(value);
    }
  }
  for (i = n_bound_to_params_args; i < nargs_required_; ++i) {
    if (bound_args_[i].is_undefined()) {
      throw error_too_few_args(i);
    }
  }
  n_bound_args_ = n_bound_to_params_args;
  kwds_dict_ = kwds;
  args_tuple_ = args;
}




PyObject* XArgs::exec_function(PyObject* args, PyObject* kwds) noexcept {
  auto cl = dt::CallLogger::function(this, args, kwds);
  try {
    bind(args, kwds);
    return (ccfn_.fn)(*this).release();

  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}

PyObject* XArgs::exec_method(PyObject* obj, PyObject* args, PyObject* kwds) noexcept {
  auto cl = dt::CallLogger::method(this, obj, args, kwds);
  try {
    bind(args, kwds);
    return (obj->*ccfn_.meth)(*this).release();
  }
  catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}


PyObject* XArgs::exec_methodv(PyObject* obj, PyObject* args, PyObject* kwds) noexcept {
  auto cl = dt::CallLogger::method(this, obj, args, kwds);
  try {
    bind(args, kwds);
    (obj->*ccfn_.methv)(*this);
    return py::None().release();
  }
  catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}


const Arg& XArgs::operator[](size_t i) const {
  xassert(i < bound_args_.size());
  return bound_args_[i];
}

size_t XArgs::num_varkwds() const noexcept {
  return n_varkwds_;
}




//------------------------------------------------------------------------------
// varargs
//------------------------------------------------------------------------------

size_t XArgs::num_varargs() const noexcept {
  return n_varargs_;
}

py::robj XArgs::vararg(size_t i) const {
  xassert(i < n_varargs_);
  auto j = static_cast<Py_ssize_t>(i + n_bound_args_);
  // PyTuple_GET_ITEM() returns a borrowed reference
  return py::robj(PyTuple_GET_ITEM(args_tuple_, j));
}

XArgs::VarArgsIterable XArgs::varargs() const noexcept {
  return XArgs::VarArgsIterable(*this);
}



XArgs::VarArgsIterable::VarArgsIterable(const XArgs& args)
  : parent_(args) {}

XArgs::VarArgsIterator XArgs::VarArgsIterable::begin() const {
  size_t i0 = parent_.n_bound_args_;
  return XArgs::VarArgsIterator(parent_, i0);
}

XArgs::VarArgsIterator XArgs::VarArgsIterable::end() const {
  size_t i1 = parent_.n_bound_args_ + parent_.n_varargs_;
  return XArgs::VarArgsIterator(parent_, i1);
}



XArgs::VarArgsIterator::VarArgsIterator(const XArgs& args, size_t i0)
  : parent_(args), pos_(static_cast<Py_ssize_t>(i0)) {}

XArgs::VarArgsIterator& XArgs::VarArgsIterator::operator++() {
  ++pos_;
  return *this;
}

py::robj XArgs::VarArgsIterator::operator*() const {
  return py::robj(PyTuple_GET_ITEM(parent_.args_tuple_, pos_));
}

bool XArgs::VarArgsIterator::operator==(const VarArgsIterator& other) const {
  return (pos_ == other.pos_);
}

bool XArgs::VarArgsIterator::operator!=(const VarArgsIterator& other) const {
  return (pos_ != other.pos_);
}





}  // namespace py
