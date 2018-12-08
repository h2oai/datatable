//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#ifndef dt_PYTHON_NAMEDTUPLE_h
#define dt_PYTHON_NAMEDTUPLE_h
#include <Python.h>
#include "python/obj.h"
#include "python/tuple.h"

namespace py {


/**
 * In Python, before a `namedtuple` can be created, its class must be
 * instantiated. This class allows you to create new "namedtuple" classes
 * dynamically and then use them when creating namedtuple objects.
 *
 * Example:
 *
 *     onamedtupletype cls("Point", {"x", "y"});
 *     onamedtuple tup(cls);
 *     tup.set(0, ofloat(1.0));
 *     tup.set(1, ofloat(2.0));
 */
class onamedtupletype {
  private:
    PyTypeObject* v;
    size_t nfields;

  public:
    onamedtupletype(const std::string& cls_name, const strvec& field_names);
    onamedtupletype(const std::string& cls_name, const std::string& cls_doc,
                    const strvec& field_names, const strvec& field_docs);
    onamedtuple(const onamedtuple&);
    onamedtuple(onamedtuple&&);
    ~onamedtupletype();

    friend class onamedtuple;
};



/**
 * This class inherits the API from `py::otuple`. The primary difference is
 * that the constructor takes an `onamedtupletype` argument rather than the
 * number of fields.
 */
class onamedtuple : public otuple {
  public:
    explicit onamedtuple(const py::onamedtupletype& type);
    onamedtuple() = default;
    onamedtuple(const onamedtuple&) = default;
    onamedtuple(onamedtuple&&) = default;
    onamedtuple& operator=(const onamedtuple&) = default;
    onamedtuple& operator=(onamedtuple&&) = default;

    // TODO: create from an existing namedtuple PyObject, extract fields, etc.
};



}  // namespace py

#endif
