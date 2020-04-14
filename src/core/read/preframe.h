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
#include "read/precolumn.h"
#include "_dt.h"
namespace dt {
namespace read {



/**
  * This class represents a "work-in-progress" Frame, while it is
  * in the process of being read from a CSV file.
  *
  * At the end of this object's lifetime, call `.to_datatable()`
  * to convert it into an actual DataTable object.
  */
class PreFrame
{
  private:
    using iterator = std::vector<PreColumn>::iterator;
    using const_iterator = std::vector<PreColumn>::const_iterator;

    std::vector<PreColumn> columns_;
    size_t nrows_;

    size_t memory_limit_;
    size_t memory_remaining_;

  public:
    PreFrame() noexcept;

    size_t ncols() const noexcept;
    size_t nrows() const noexcept;
    void set_ncols(size_t ncols);
    void set_nrows(size_t nrows);

    // Column iterator / access
    const_iterator begin() const;
    const_iterator end() const;
    iterator begin();
    iterator end();
    PreColumn& column(size_t i) &;
    const PreColumn& column(size_t i) const &;

    std::vector<PT> get_ptypes() const;
    void save_ptypes(std::vector<PT>& types) const;
    bool are_same_ptypes(std::vector<PT>& types) const;
    void set_ptypes(const std::vector<PT>& types);
    void reset_ptypes();
    const char* print_ptypes() const;

    size_t n_columns_in_output() const;
    size_t n_columns_in_buffer() const;
    size_t n_columns_to_reread() const;
    size_t total_allocsize() const;

    void set_memory_bound(size_t);
    void use_memory_quota(size_t);

    std::unique_ptr<DataTable> to_datatable() &&;
};




}}  // namespace dt::read
#endif
