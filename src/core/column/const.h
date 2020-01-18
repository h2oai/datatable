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
#ifndef dt_COLUMN_CONST_h
#define dt_COLUMN_CONST_h
#include "column/virtual.h"
namespace dt {


class Const_ColumnImpl : public Virtual_ColumnImpl {
  public:
    using Virtual_ColumnImpl::Virtual_ColumnImpl;
    static Column make_na_column(size_t nrows);
    static Column make_bool_column(size_t nrows, bool value);
    static Column make_int_column(size_t nrows, int64_t value, SType stype = SType::VOID);
    static Column make_float_column(size_t nrows, double value, SType stype = SType::FLOAT64);
    static Column make_string_column(size_t nrows, CString value, SType stype = SType::STR32);
    static Column from_1row_column(const Column& col);

    void repeat(size_t ntimes, Column& out) override;
};



/**
  * Virtual column containing only NA values. This column may have
  * any stype, including SType::VOID (in fact, this is the only
  * column that allows VOID stype).
  */
class ConstNa_ColumnImpl : public Const_ColumnImpl {
  public:
    ConstNa_ColumnImpl(size_t nrows, SType stype = SType::VOID);

    bool get_element(size_t, int8_t*)   const override;
    bool get_element(size_t, int16_t*)  const override;
    bool get_element(size_t, int32_t*)  const override;
    bool get_element(size_t, int64_t*)  const override;
    bool get_element(size_t, float*)    const override;
    bool get_element(size_t, double*)   const override;
    bool get_element(size_t, CString*)  const override;
    bool get_element(size_t, py::robj*) const override;

    ColumnImpl* clone() const override;
    void materialize(Column&, bool) override;
    void na_pad(size_t nrows, Column&) override;
};





}  // namespace dt
#endif
