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
class RowIndex;

namespace dt {
namespace expr {
  using std::size_t;

  class Head;
  class Expr;
  class Workframe;

  using strvec = std::vector<std::string>;
  using ptrHead = std::unique_ptr<Head>;
  using vecExpr = std::vector<Expr>;

  // TODO: remove
  class base_expr;


  // The `Grouping` enum describes how a column or a set of columns
  // behave wrt a group-by structure on the frame.
  //
  // SCALAR
  //   Indicates that the column is a scalar, which conforms to any
  //   frame size. Such column can be resized as necessary.
  //
  // GtoONE
  //   Each group is mapped to exactly 1 row. This grouping mode is
  //   common as a result of "reduce" operations such as `sum`, `sd`,
  //   `min`, etc. A column with this level may need to be expanded
  //   in order to become conformable with any full-sized column.
  //
  // GtoFEW
  //   Each group is mapped to 0 <= ... <= `groupsize` rows. This
  //   mode is uncommon. If it needs to be upcasted to the
  //   "full-sized" level, any missing entries are filled with NAs.
  //
  // GtoALL
  //   Each group is mapped to exactly `groupsize` rows. This is the
  //   most common grouping mode. Any simple column, or a function of
  //   a simple column will be using this mode. Few groupby functions
  //   may use this mode too
  //
  // GtoANY
  //   Groups may be mapped to any number of rows, including having
  //   more rows than `groupsize`. This is the rarest grouping mode.
  //
  enum Grouping : size_t {
    SCALAR = 0,
    GtoONE = 1,
    GtoFEW = 2,
    GtoALL = 3,
    GtoANY = 4,
  };


  // The `Kind` enum is the value returned by `Expr::get_expr_kind()`.
  // This value roughly corresponds to the type of the Expr, and is
  // used in contexts where we may need to query such type.
  //
  enum Kind {
    Unknown,
    None,
    Bool,
    Int,
    Float,
    Str,
    Type,
    Func,
    List,
    Frame,
    SliceAll,
    SliceInt,
    SliceStr
  };

}}


//
// OBSOLETE
//
namespace dt {
  class EvalContext;
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
