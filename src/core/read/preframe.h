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
#ifndef dt_READ_PREFRAME_h
#define dt_READ_PREFRAME_h
#include "read/column.h"
#include "_dt.h"
namespace dt {
namespace read {



class PreFrame {
  using ptvec = std::vector<PT>;

  private:
    std::vector<dt::read::Column> cols;
    size_t nrows;

  public:
    PreFrame() noexcept;

    size_t size() const noexcept;
    size_t get_nrows() const noexcept;
    void set_nrows(size_t nrows);

    Column& operator[](size_t i) &;
    const Column& operator[](size_t i) const &;
    std::vector<std::string> get_names() const;

    void add_columns(size_t n);

    ptvec getTypes() const;
    void saveTypes(ptvec& types) const;
    bool sameTypes(ptvec& types) const;
    void setTypes(const ptvec& types);
    void setType(PT type);
    const char* printTypes() const;

    size_t nColumnsInOutput() const;
    size_t nColumnsInBuffer() const;
    size_t nColumnsToReread() const;
    size_t nStringColumns() const;
    size_t totalAllocSize() const;
};



}}  // namespace dt::read
#endif
