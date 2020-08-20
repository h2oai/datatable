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
#ifndef dt_PYTHON_ARGS_h
#define dt_PYTHON_ARGS_h
#include <iterator>        // std::input_iterator_tag
#include <unordered_map>   // std::unordered_map
#include <vector>          // std::vector
#include "python/arg.h"
#include "utils/assert.h"
#include "utils/exceptions.h"

namespace py {


class ArgParent {
  public:
    virtual ~ArgParent() = default;
    virtual std::string descriptive_name(bool lowercase = false) const = 0;
    virtual const char* arg_name(size_t i) const = 0;
    virtual size_t n_positional_args() const = 0;
    virtual size_t n_positional_or_keyword_args() const = 0;
    virtual size_t n_keyword_args() const = 0;
    virtual bool has_varargs() const = 0;
    virtual bool has_varkwds() const = 0;
};



//------------------------------------------------------------------------------
// GSArgs
//------------------------------------------------------------------------------

/**
 * Helper class for getters/setters in XTypeMaker.
 */
class GSArgs {
  public:
    const char* name;
    const char* doc;
    const char* class_name;
    Arg _arg;

    GSArgs(const char* name_, const char* doc_=nullptr)
      : name(name_),
        doc(doc_),
        class_name(nullptr),
        _arg(std::string("`.") + name_ + "`") {}
};




//------------------------------------------------------------------------------
// PKArgs
//------------------------------------------------------------------------------
class VarArgsIterator;
class VarArgsIterable;
class VarKwdsIterator;
class VarKwdsIterable;


/**
 * Helper class for XTypeMaker: it encapsulates arguments passed to a function
 * and helps verify / parse them. This is the base class for a family of
 * args-related functions, each designed to handle different calling
 * convention.
 */
class PKArgs : public ArgParent {
  private:
    const char* cls_name;
    const char* fun_name;
    const char* fun_doc;
    mutable const char* full_name;

    const size_t n_posonly_args;
    const size_t n_pos_kwd_args;
    const size_t n_all_args;
    const bool   has_varargs_;
    const bool   has_varkwds_;
    bool         has_renamed_args;
    size_t : 40;
    const std::vector<const char*> arg_names;

    // Runtime arguments
    std::vector<Arg> bound_args;
    std::unordered_map<PyObject*, size_t> kwd_map;
    size_t n_varargs;
    size_t n_varkwds;
    PyObject* args_tuple;  // for var-args iteration
    PyObject* kwds_dict;   // for var-kwds iteration

  public:
    /**
     * npo:   number of positional-only arguments
     * npk:   number of positional-or-keyword arguments
     * nko:   number of keyword-only arguments
     * vargs: positional var-args allowed? (`*args` in python)
     * vkwds: var-keywords allowed? (`**kwds` in python)
     * names: list of argument names
     * name:  function name
     * doc:   doc-string
     */
    PKArgs(size_t npo, size_t npk, size_t nko, bool vargs, bool vkwds,
           std::initializer_list<const char*> names,
           const char* name = nullptr, const char* doc = nullptr);
    PKArgs(const PKArgs&) = delete;
    PKArgs(PKArgs&&) = delete;
    ~PKArgs() override;
    void add_synonym_arg(const char* new_name, const char* old_name);

    void bind(PyObject* _args, PyObject* _kws);

    PyObject* exec_function(
        PyObject* args, PyObject* kwds,
        oobj (*fn)(const PKArgs&)) noexcept;

    PyObject* exec_function(
        PyObject* args, PyObject* kwds,
        void (*fn)(const PKArgs&)) noexcept;


    //---- API for XTypeMaker ----------
    void set_class_name(const char* name);

    // Each `Args` object describes a certain function, or method in a class.
    // This will return the name of that function/method, in the form "foo()"
    // or "Class.foo()".
    const char* get_long_name() const;

    const char* get_class_name() const;
    const char* get_short_name() const;
    const char* get_docstring() const;

    /**
     * Returns the name of argument `i`, which will usually be in one of the
     * following forms (depending on whether the argument is positional or
     * named):
     *    "First argument in Class.method()"
     *    "5th argument in Class.method()"
     *    "Argument `foo` in Class.method()"
     * This name can also be obtained as
     *    args[i].name()
     */
    const char* arg_name(size_t i) const override;
    std::string descriptive_name(bool lowercase = false) const override;
    size_t n_positional_args() const override;
    size_t n_positional_or_keyword_args() const override;
    size_t n_keyword_args() const override;
    bool has_varargs() const override;
    bool has_varkwds() const override;

    //---- User API --------------------
    void check_posonly_args() const;
    void check_required_args(size_t n_required_args) const;
    const Arg& operator[](size_t i) const;

    size_t num_vararg_args() const noexcept;
    size_t num_varkwd_args() const noexcept;
    VarKwdsIterable varkwds() const noexcept;
    VarArgsIterable varargs() const noexcept;

    template <typename T> T get(size_t i) const;
    template <typename T> T get(size_t i, T default_value) const;

  private:
    size_t _find_kwd(PyObject* kwd);

    friend class VarArgsIterable;
    friend class VarArgsIterator;
    friend class VarKwdsIterator;
};



//------------------------------------------------------------------------------
// Helper iterator classes
//------------------------------------------------------------------------------

class VarKwdsIterator {
  public:
    using value_type = std::pair<py::robj, py::robj>;
    using category_type = std::input_iterator_tag;

  private:
    const PKArgs& parent;
    Py_ssize_t pos;
    value_type curr_value;

  public:
    VarKwdsIterator(const PKArgs&, Py_ssize_t i0);
    VarKwdsIterator(const VarKwdsIterator&) = default;
    VarKwdsIterator& operator=(const VarKwdsIterator&) = delete;

    VarKwdsIterator& operator++();
    value_type operator*() const;
    bool operator==(const VarKwdsIterator&) const;
    bool operator!=(const VarKwdsIterator&) const;

  private:
    void advance();
};


class VarKwdsIterable {
  private:
    const PKArgs& parent;

  public:
    using iterator = VarKwdsIterator;

    VarKwdsIterable(const PKArgs&);
    iterator begin() const;
    iterator end() const;
};


class VarArgsIterator {
  public:
    using value_type = py::robj;
    using category_type = std::input_iterator_tag;

  private:
    const PKArgs& parent;
    size_t pos;

  public:
    VarArgsIterator(const PKArgs&, size_t i0);
    VarArgsIterator(const VarArgsIterator&) = default;
    VarArgsIterator& operator=(const VarArgsIterator&) = delete;

    VarArgsIterator& operator++();
    value_type operator*() const;
    bool operator==(const VarArgsIterator&) const;
    bool operator!=(const VarArgsIterator&) const;

  private:
    void advance();
};


class VarArgsIterable {
  private:
    const PKArgs& parent;

  public:
    using iterator = VarArgsIterator;

    VarArgsIterable(const PKArgs&);
    iterator begin() const;
    iterator end() const;
};



//------------------------------------------------------------------------------
// templates
//------------------------------------------------------------------------------

template <typename T>
T PKArgs::get(size_t i) const {
  if (bound_args[i].is_none_or_undefined()) {
    throw TypeError() << "Argument `" << arg_names[i] << "` is missing";
  }
  return static_cast<T>(bound_args[i]);
}

template <typename T>
T PKArgs::get(size_t i, T default_value) const {
  return bound_args[i].is_none_or_undefined()
          ? default_value
          : static_cast<T>(bound_args[i]);
}



}  // namespace py

#endif
