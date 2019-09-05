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
#ifndef dt_EXPR_DECLARATIONS_h
#define dt_EXPR_DECLARATIONS_h
#include <vector>
#include <string>
#include <memory>

class Column;
class DataTable;

namespace dt {
namespace expr {
  class Head;
  class Expr;
  class Outputs;

  using strvec = std::vector<std::string>;
  using ptrHead = std::unique_ptr<Head>;
  using vecExpr = std::vector<Expr>;

  class base_expr;
}}


namespace dt {
  class workframe;
  class by_node;
  class collist;

  using by_node_ptr = std::unique_ptr<by_node>;
  using collist_ptr = std::unique_ptr<collist>;

  enum class GroupbyMode : uint8_t {
    NONE   = 0,
    GtoONE = 1,
    GtoALL = 2,
    GtoANY = 3
  };
}
#endif
