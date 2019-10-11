//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#ifndef dt_READ_COLUMNS_h
#define dt_READ_COLUMNS_h
#include <vector>           // std::vector
#include <memory>           // std::unique_ptr
#include "read/column.h"

enum PT : uint8_t;

namespace dt {
namespace read {



class Columns {
  using PTlist = std::unique_ptr<PT[]>;

  private:
    std::vector<Column> cols;
    size_t nrows;

  public:
    Columns() noexcept;

    size_t size() const noexcept;
    size_t get_nrows() const noexcept;
    void set_nrows(size_t nrows);

    Column& operator[](size_t i) &;
    const Column& operator[](size_t i) const &;
    std::vector<std::string> get_names() const;

    void add_columns(size_t n);

    PTlist getTypes() const;
    void saveTypes(PTlist& types) const;
    bool sameTypes(PTlist& types) const;
    void setTypes(const PTlist& types);
    void setType(PT type);
    const char* printTypes() const;

    size_t nColumnsInOutput() const;
    size_t nColumnsInBuffer() const;
    size_t nColumnsToReread() const;
    size_t nStringColumns() const;
    size_t totalAllocSize() const;
};


}  // namespace read
}  // namespace dt

#endif
