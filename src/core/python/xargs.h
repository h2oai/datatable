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
#ifndef dt_PYTHON_XARGS_h
#define dt_PYTHON_XARGS_h
#include <iterator>        // std::input_iterator_tag
#include <string>          // std::string
#include <unordered_map>   // std::unordered_map
#include <vector>          // std::vector
#include "utils/macros.h"
#include "python/arg.h"
#include "python/args.h"   // ArgParent
namespace py {



/**
  * Class that encapsulates arguments passed to a python function
  * and helps verify / parse them.
  */
class XArgs : public ArgParent {
  using impl_function_t = oobj(*)(const XArgs&);
  using impl_method_t = oobj(PyObject::*)(const XArgs&);
  using impl_methodv_t = void(PyObject::*)(const XArgs&);
  private:
    union {
        impl_function_t  fn;
        impl_method_t    meth;
        impl_methodv_t   methv;
    } ccfn_;
    PyCFunctionWithKeywords pyfn_;
    std::string class_name_;
    std::string function_name_;
    const char* docstring_;
    size_t classId_;

    std::vector<const char*> arg_names_;
    size_t nargs_required_;
    size_t nargs_posonly_;
    size_t nargs_pos_kwd_;
    size_t nargs_kwdonly_;
    size_t nargs_all_;
    bool has_varargs_;
    bool has_varkwds_;
    bool has_renamed_args_;
    int : 8;
    int info_;  // custom user info that can be stored inside XArgs

    // Runtime arguments
    std::vector<Arg> bound_args_;
    std::unordered_map<PyObject*, size_t> kwd_map_;
    size_t n_bound_args_;
    size_t n_varargs_;
    size_t n_varkwds_;
    PyObject* args_tuple_;  // for var-args iteration
    PyObject* kwds_dict_;   // for var-kwds iteration

  public:
    XArgs(impl_function_t fn);
    XArgs(impl_method_t method, size_t classId);
    XArgs(impl_methodv_t method, size_t classId);
    XArgs(const XArgs&) = delete;
    XArgs(XArgs&&) = delete;
    XArgs* pyfunction(PyCFunctionWithKeywords f);  // "private"

    XArgs* name(const char* name);
    XArgs* arg_names(std::initializer_list<const char*> names);
    XArgs* n_required_args(size_t n);
    XArgs* n_positional_args(size_t n);
    XArgs* n_positional_or_keyword_args(size_t n);
    XArgs* n_keyword_args(size_t n);
    XArgs* allow_varargs();
    XArgs* allow_varkwds();
    XArgs* docs(const char*);
    XArgs* add_info(int);
    XArgs* add_synonym_arg(const char* new_name, const char* old_name);
    XArgs* set_class_name(const char* name);

    size_t n_positional_args() const override;
    size_t n_positional_or_keyword_args() const override;
    size_t n_keyword_args() const override;
    bool has_varargs() const override;
    bool has_varkwds() const override;
    const char* arg_name(size_t i) const override;

    // Return the name of the method/function:
    //   proper_name()
    //       Simple name like "sin" or "cbind"
    //
    //   qualified_name()
    //       Produces module+[class]+name, like "datatable.math.sin" or
    //       "datatable.Frame.cbind".
    //
    //   descriptive_name()
    //       Returns qualified name with a description:
    //       "Function `datatable.math.sin()`" or
    //       "Method `datatable.Frame.cbind()`". For constructor, it
    //       returns "`datatable.Frame()` constructor".
    //
    const std::string& proper_name() const;
    std::string qualified_name() const;
    std::string descriptive_name(bool lowercase = false) const override;

    static std::vector<XArgs*>& store();
    PyMethodDef get_method_def();
    PyCFunctionWithKeywords get_pyfunction();
    const char* get_docstring() const;

    // void add_synonym_arg(const char* new_name, const char* old_name);


    PyObject* exec_function(PyObject* args, PyObject* kwds) noexcept;
    PyObject* exec_method(PyObject* self, PyObject* args, PyObject* kwds) noexcept;
    PyObject* exec_methodv(PyObject* self, PyObject* args, PyObject* kwds) noexcept;

    // //---- API for XTypeMaker ----------
    // void set_class_name(const char* name);

    // // Each `Args` object describes a certain function, or method in a class.
    // // This will return the name of that function/method, in the form "foo()"
    // // or "Class.foo()".
    // const char* get_long_name() const;

    // const char* get_class_name() const;
    // const char* get_short_name() const;
    // const char* get_docstring() const;

    // const char* get_arg_short_name(size_t i) const;

    // //---- User API --------------------
    const Arg& operator[](size_t i) const;
    size_t num_varargs() const noexcept;
    size_t num_varkwds() const noexcept;
    py::robj vararg(size_t i) const;
    int get_info() const;
    size_t get_class_id() const { return classId_; }


    class VarArgsIterator {
      private:
        const XArgs& parent_;
        Py_ssize_t pos_;  // position within parent_'s arg_tuple_

      public:
        using value_type = py::robj;
        using category_type = std::input_iterator_tag;
        VarArgsIterator(const XArgs& args, size_t i0);
        VarArgsIterator(const VarArgsIterator&) = default;
        VarArgsIterator& operator=(const VarArgsIterator&) = delete;
        VarArgsIterator& operator++();
        value_type operator*() const;
        bool operator==(const VarArgsIterator& other) const;
        bool operator!=(const VarArgsIterator& other) const;
    };

    class VarArgsIterable {
      private:
        const XArgs& parent_;
      public:
        using iterator = VarArgsIterator;
        VarArgsIterable(const XArgs&);
        iterator begin() const;
        iterator end() const;
    };

    VarArgsIterable varargs() const noexcept;


    // VarKwdsIterable varkwds() const noexcept;

    // template <typename T> T get(size_t i) const;
    // template <typename T> T get(size_t i, T default_value) const;

  private:
    XArgs();
    void finish_initialization();

    Error error_too_few_args(size_t) const;
    Error error_too_many_args(size_t) const;

    void bind(PyObject* args, PyObject* kwds);
    size_t find_kwd(PyObject* kwd);

    // friend class VarKwdsIterator;
};


// #define RESULT_OF(fn)
//     typename std::result_of<decltype(fn)(CLASS_OF(fn), const XArgs&)>::type


#define ARGS_NAME  PASTE_TOKENS(args_, __LINE__)
#define DECLARE_PYFN(fn)                                                       \
    static py::XArgs* ARGS_NAME = (new py::XArgs(fn))                          \
      ->pyfunction(                                                            \
          [](PyObject*, PyObject* args, PyObject* kwds) -> PyObject* {         \
            return ARGS_NAME->exec_function(args, kwds);                       \
          })

#define DECLARE_METHOD(fn)                                                     \
    static py::XArgs* ARGS_NAME = (new py::XArgs(                              \
        reinterpret_cast<oobj(PyObject::*)(const XArgs&)>(fn),                 \
        typeid(CLASS_OF(fn)).hash_code())                                      \
      )->pyfunction(                                                           \
          [](PyObject* self, PyObject* args, PyObject* kwds) -> PyObject* {    \
            return ARGS_NAME->exec_method(self, args, kwds);                   \
          })

#define DECLARE_METHODv(fn)                                                    \
    static py::XArgs* ARGS_NAME = (new py::XArgs(                              \
        reinterpret_cast<void(PyObject::*)(const XArgs&)>(fn),                 \
        typeid(CLASS_OF(fn)).hash_code())                                      \
      )->pyfunction(                                                           \
          [](PyObject* self, PyObject* args, PyObject* kwds) -> PyObject* {    \
            return ARGS_NAME->exec_methodv(self, args, kwds);                  \
          })

#define INIT_METHODS_FOR_CLASS(CLASS)                                          \
    do {                                                                       \
      for (XArgs* xargs : XArgs::store()) {                                    \
        if (xargs->get_class_id() == typeid(CLASS).hash_code()) {              \
          xt.add(xargs->get_pyfunction(), xargs, py::XTypeMaker::method_tag);  \
        }                                                                      \
      }                                                                        \
    } while(0)



}  // namespace py
#endif
