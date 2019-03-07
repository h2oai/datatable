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
#ifndef dt_EXPR_BASE_EXPR_h
#define dt_EXPR_BASE_EXPR_h
#include "column.h"
#include "expr/workframe.h"
#include "python/ext_type.h"


namespace dt {

enum exprCode : size_t {
  COL      = 1,
  BINOP    = 2,
  LITERAL  = 3,
  UNOP     = 4,
  CAST     = 5,
  UNREDUCE = 6,
  NUREDUCE = 7,
  STRINGFN = 8,
};

enum class biop : size_t {
  PLUS         = 1,
  MINUS        = 2,
  MULTIPLY     = 3,
  DIVIDE       = 4,
  INT_DIVIDE   = 5,
  POWER        = 6,
  MODULO       = 7,
  LOGICAL_AND  = 8,
  LOGICAL_OR   = 9,
  LEFT_SHIFT   = 10,
  RIGHT_SHIFT  = 11,
  REL_EQ       = 12,  // ==
  REL_NE       = 13,  // !=
  REL_GT       = 14,  // >
  REL_LT       = 15,  // <
  REL_GE       = 16,  // >=
  REL_LE       = 17,  // <=
};

enum class unop : size_t {
  ISNA   = 1,
  MINUS  = 2,
  PLUS   = 3,
  INVERT = 4,
  ABS    = 5,
  EXP    = 6,
  LOGE   = 7,
  LOG10  = 8,
  LEN    = 9,
};

enum class strop : size_t {
  RE_MATCH = 1,
};

class base_expr;
using pexpr = std::unique_ptr<base_expr>;
using colptr = std::unique_ptr<Column>;


//------------------------------------------------------------------------------
// dt::base_expr
//------------------------------------------------------------------------------

class base_expr {
  public:
    base_expr();
    virtual ~base_expr();
    virtual SType resolve(const workframe&) = 0;
    virtual GroupbyMode get_groupby_mode(const workframe&) const = 0;
    virtual colptr evaluate_eager(workframe&) = 0;

    virtual bool is_column_expr() const;
    virtual bool is_negated_expr() const;
    virtual pexpr get_negated_expr();
    virtual size_t get_col_index(const workframe&);
};


class expr_column : public base_expr {
  private:
    size_t frame_id;
    size_t col_id;
    py::oobj col_selector;

  public:
    expr_column(size_t dfid, const py::robj& col);

    size_t get_frame_id() const noexcept;
    size_t get_col_index(const workframe&) override;
    bool is_column_expr() const override;
    SType resolve(const workframe&) override;
    GroupbyMode get_groupby_mode(const workframe&) const override;
    colptr evaluate_eager(workframe&) override;
};


pexpr expr_string_fn(size_t op, pexpr&& arg, py::oobj params);



}  // namespace dt
namespace py {


class base_expr : public PyObject {
  private:
    dt::base_expr* expr;  // owned

  public:
    class Type : public ExtType<base_expr> {
      public:
        static PKArgs args___init__;
        static const char* classname();
        static const char* classdoc();
        static bool is_subclassable() { return false; }
        static void init_methods_and_getsets(Methods&, GetSetters&);
    };

    void m__init__(PKArgs&);
    void m__dealloc__();

    dt::pexpr release();
};




}  // namespace py


bool is_PyBaseExpr(const py::_obj&);

#endif
