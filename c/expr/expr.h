//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#ifndef dt_EXPR_EXPR_h
#define dt_EXPR_EXPR_h
#include "column.h"
#include "expr/workframe.h"
#include "python/ext_type.h"
namespace dt {
namespace expr {

class base_expr;
using pexpr = std::unique_ptr<base_expr>;
using colptr = std::unique_ptr<Column>;


// The values in this enum must be kept in sync with Python enum OpCodes in
// file "datatable/expr/expr.py"
enum class Op : size_t {
    // Misc
    NOOP = 0,
    COL = 1,
    CAST = 2,

    // Unary
    UPLUS = 101,
    UMINUS = 102,
    INVERT = 103,
    ISNA = 104,
    ABS = 105,
    EXP = 106,
    LOGE = 107,
    LOG10 = 108,

    // Binary
    PLUS = 201,
    MINUS = 202,
    MULTIPLY = 203,
    DIVIDE = 204,
    INTDIV = 205,
    MODULO = 206,
    POWER = 207,
    AND = 208,
    XOR = 209,
    OR = 210,
    LSHIFT = 211,
    RSHIFT = 212,
    EQ = 213,
    NE = 214,
    LT = 215,
    GT = 216,
    LE = 217,
    GE = 218,

    // String
    LEN = 301,
    RE_MATCH = 302,

    // Reducers
    MEAN = 401,
    MIN = 402,
    MAX = 403,
    STDEV = 404,
    FIRST = 405,
    SUM = 406,
    COUNT = 407,
    COUNT0 = 408,
    MEDIAN = 409,
};

static constexpr size_t BINOP_FIRST = static_cast<size_t>(Op::PLUS);
static constexpr size_t BINOP_LAST  = static_cast<size_t>(Op::GE);
static constexpr size_t UNOP_FIRST = static_cast<size_t>(Op::UPLUS);
static constexpr size_t UNOP_LAST  = static_cast<size_t>(Op::INVERT);
static constexpr size_t REDUCER_FIRST = static_cast<size_t>(Op::MEAN);
static constexpr size_t REDUCER_LAST  = static_cast<size_t>(Op::MEDIAN);
static constexpr size_t REDUCER_COUNT = REDUCER_LAST - REDUCER_FIRST + 1;



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





}}  // namespace dt::expr




// pexpr expr_string_fn(size_t op, pexpr&& arg, py::oobj params);




#endif
