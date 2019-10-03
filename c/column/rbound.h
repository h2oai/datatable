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
#ifndef dt_COLUMN_RBOUND_h
#define dt_COLUMN_RBOUND_h
#include <memory>
#include <vector>
#include "column/virtual.h"
namespace dt {


/**
  *
  */
class Rbound_ColumnImpl : public Virtual_ColumnImpl {
  private:
    std::vector<Column> columns_;

  public:
    Rbound_ColumnImpl(const colvec& columns);

    ColumnImpl* clone() const override;
    // ColumnImpl* materialize() override;

    bool get_element(size_t i, int8_t* out)   const override;
    bool get_element(size_t i, int16_t* out)  const override;
    bool get_element(size_t i, int32_t* out)  const override;
    bool get_element(size_t i, int64_t* out)  const override;
    bool get_element(size_t i, float* out)    const override;
    bool get_element(size_t i, double* out)   const override;
    bool get_element(size_t i, CString* out)  const override;
    bool get_element(size_t i, py::robj* out) const override;
};



}  // namespace dt
#endif
