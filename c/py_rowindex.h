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
#ifndef dt_PY_ROWINDEX_h
#define dt_PY_ROWINDEX_h
#include "python/ext_type.h"
#include "python/obj.h"
#include "rowindex.h"
namespace py {


class orowindex : public oobj {
  public:
    orowindex(const RowIndex& rowindex);
    orowindex() = default;
    orowindex(const orowindex&) = default;
    orowindex(orowindex&&) = default;
    orowindex& operator=(const orowindex&) = default;
    orowindex& operator=(orowindex&&) = default;

    static bool check(PyObject*);

    // Declare class orowindex::pyobject
    struct pyobject;
};



struct orowindex::pyobject : public PyObject {
  RowIndex* ri;  // owned ref

  class Type : public ExtType<pyobject> {
    public:
      static PKArgs args___init__;
      static NoArgs args_to_list;
      static const char* classname();
      static const char* classdoc();
      static bool is_subclassable();
      static void init_methods_and_getsets(Methods&, GetSetters&);
  };

  void m__init__(PKArgs&);
  void m__dealloc__();
  oobj get_type() const;
  oobj get_nrows() const;
  oobj get_min() const;
  oobj get_max() const;

  oobj to_list(const NoArgs&);
};



}  // namespace py
#endif
