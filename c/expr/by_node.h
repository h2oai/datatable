//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#ifndef dt_EXPR_BY_NODE_h
#define dt_EXPR_BY_NODE_h
#include "expr/declarations.h"
#include "python/obj.h"
#include "python/xobject.h"
#include "datatable.h"
#include "groupby.h"         // Groupby
namespace dt {




//------------------------------------------------------------------------------
// dt::by_node
//------------------------------------------------------------------------------

class by_node {
  private:
    using exprptr = std::unique_ptr<dt::expr::base_expr>;
    struct column_descriptor {
      size_t      index;
      exprptr     expr;
      std::string name;
      bool        descending;
      bool        sort_only;
      size_t : 48;

      column_descriptor(size_t i, std::string&& name_, bool desc, bool sort);
      column_descriptor(exprptr&& e, std::string&& name_, bool desc, bool sort);
    };

    std::vector<column_descriptor> cols;
    size_t n_group_columns;

  public:
    by_node();
    ~by_node();

    void add_groupby_columns(expr::EvalContext&, collist_ptr&&);
    void add_sortby_columns(expr::EvalContext&, collist_ptr&&);

    explicit operator bool() const;
    bool has_group_column(size_t i) const;
    void create_columns(expr::EvalContext&);
    void execute(expr::EvalContext&) const;

  private:
    void _add_columns(expr::EvalContext& ctx, collist_ptr&& cl, bool isgrp);
};



}  // namespace dt
#endif
