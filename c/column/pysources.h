//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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
#ifndef dt_COLUMN_PYSOURCES_h
#define dt_COLUMN_PYSOURCES_h
#include "column/virtual.h"
#include "python/list.h"
namespace dt {



/**
  * Virtual column that wraps a simple python list.
  */
class PyList_ColumnImpl : public Virtual_ColumnImpl {
  private:
    py::olist list_;

  public:
    explicit PyList_ColumnImpl(const py::olist&);

    ColumnImpl* clone() const override;
    bool get_element(size_t, py::robj*) const override;
};



/**
  * Virtual column whose source is a list of python tuples, and it
  * outputs elements of those tuples at some specific index.
  */
class PyTupleList_ColumnImpl : public Virtual_ColumnImpl {
  private:
    py::olist tuple_list_;
    size_t index_;

  public:
    explicit PyTupleList_ColumnImpl(const py::olist&, size_t index);

    ColumnImpl* clone() const override;
    bool get_element(size_t, py::robj*) const override;
};



/**
  * Virtual column whose source is a list of python dictionaries, and
  * the column outputs elements of those dictionaries corresponding
  * to some fixed key.
  */
class PyDictList_ColumnImpl : public Virtual_ColumnImpl {
  private:
    py::olist dict_list_;
    py::oobj key_;

  public:
    explicit PyDictList_ColumnImpl(const py::olist&, py::oobj key);

    ColumnImpl* clone() const override;
    bool get_element(size_t, py::robj*) const override;
};




}  // namespace dt
#endif
