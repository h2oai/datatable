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
#ifndef dt_COLUMN_REPEATED_h
#define dt_COLUMN_REPEATED_h
#include <memory>
#include "column_impl.h"
namespace dt {


/**
  * Virtual column representing the `arg` column repeated n times.
  */
class Repeated_ColumnImpl : public ColumnImpl {
  private:
    size_t mod;
    Column arg;  // arg.nrows() == mod

  public:
    Repeated_ColumnImpl(Column&&, size_t ntimes);

    bool is_virtual() const noexcept override;
    ColumnImpl* shallowcopy() const override;
    void repeat(size_t ntimes, bool inplace, Column& out) override;
    // ColumnImpl* materialize() override;

    bool get_element(size_t, int8_t*)   const override;
    bool get_element(size_t, int16_t*)  const override;
    bool get_element(size_t, int32_t*)  const override;
    bool get_element(size_t, int64_t*)  const override;
    bool get_element(size_t, float*)    const override;
    bool get_element(size_t, double*)   const override;
    bool get_element(size_t, CString*)  const override;
    bool get_element(size_t, py::robj*) const override;
};




}  // namespace dt
#endif
