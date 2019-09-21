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
#ifndef dt_EXPR_REFLIST_h
#define dt_EXPR_REFLIST_h
#include <string>
#include <vector>
#include "expr/workframe.h"
namespace dt {
namespace expr {



/**
  * Simple structure that is used in `RefList`. This is a reference
  * to a column `column_index` within the frame `frame_id` in the
  * current EvalContext.
  */
struct RefColumn {
  size_t frame_id;
  size_t column_index;
};


/**
  * The list of columns, captured by reference. This class is
  * primarily used as a return type for `Expr::evaluate_ref()`.
  *
  * Note that we allow the list of references to be intermixed
  * with non-reference columns. These are collected into the
  * `outputs` list (which may be empty otherwise).
  *
  * Specifically, when `items[i].frame_id == -1` then this item is
  * considered not a reference column. Instead, its `.column_index`
  * contains the index into the `outputs` array that contains the
  * content of this column. The entries in `outputs` array may be
  * of 2 types too: with a valid `.column` and with an empty one.
  * The former case means a regular computed column, the latter is
  * used for new columns that will be added to the
  *
  */
class RefList {
  private:
    std::vector<RefColumn> items;
    Workframe outputs;

  public:

};



}}  // namespace dt::expr
#endif
