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
#ifndef dt_EXPR_OUTPUTS_h
#define dt_EXPR_OUTPUTS_h
#include <string>
#include <vector>
#include "column.h"
namespace dt {
namespace expr {


class Outputs {
  private:
    // The `grouping_level` describes how the output column was
    // constructed w.r.t. grouped input frame (and if it wasn't
    // explicitly grouped, presume that the entire frame was one
    // single big group).
    //   0 - each group is mapped to 0 rows (shouldn't happen?)
    //   1 - each group is mapped to exactly 1 row
    //   2 - each group is mapped to 0 <= ... <= `groupsize` rows
    //   3 - each group is mapped to `groupsize` rows
    //   4 - groups may be mapped to any number of rows,
    //       including having more rows than `groupsize`
    //
    // The most common levels are 1 and 3.
    //
    struct Output {
      Column       column;
      std::string  name;
      size_t       grouping_level;

      Output(Column&&, std::string&&, size_t);
    };

    std::vector<Output> items;

  public:
    static constexpr size_t GroupToNone = 0;
    static constexpr size_t GroupToOne  = 1;
    static constexpr size_t GroupToSome = 2;
    static constexpr size_t GroupToAll  = 3;
    static constexpr size_t GroupToMany = 4;

    Outputs() = default;
    Outputs(const Outputs&) = default;
    Outputs(Outputs&&) = default;

    Outputs& add(Column&& col, std::string&& name, size_t group_level);
    Outputs& add(Column&& col, size_t group_level);
    Outputs& add(Column&& col);
    size_t size() const noexcept;
    void append(Outputs&&);
    void apply_name(const std::string& name);

    strvec release_names();
    colvec release_columns();
};



}}  // namespace dt::expr
#endif
