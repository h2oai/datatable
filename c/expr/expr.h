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

static constexpr size_t UNOP_FIRST    = 101;
static constexpr size_t UNOP_LAST     = 111;
static constexpr size_t BINOP_FIRST   = 201;
static constexpr size_t BINOP_LAST    = 218;
static constexpr size_t STRING_FIRST  = 301;
static constexpr size_t REDUCER_FIRST = 401;
static constexpr size_t REDUCER_LAST  = 409;
static constexpr size_t MATH_FIRST    = 501;
static constexpr size_t MATH_LAST     = 542;
static constexpr size_t UNOP_COUNT    = UNOP_LAST - UNOP_FIRST + 1;
static constexpr size_t BINOP_COUNT   = BINOP_LAST - BINOP_FIRST + 1;
static constexpr size_t REDUCER_COUNT = REDUCER_LAST - REDUCER_FIRST + 1;

// The values in this enum must be kept in sync with Python enum OpCodes in
// file "datatable/expr/expr.py"
enum class Op : size_t {
  // Misc
  NOOP = 0,
  COL = 1,
  CAST = 2,

  // Unary
  UPLUS = UNOP_FIRST,
  UMINUS,
  UINVERT,
  ISFINITE,
  ISINF,
  ISNA,
  ABS,
  CEIL,
  FLOOR,
  TRUNC,
  LEN = UNOP_LAST,

  // Binary
  PLUS = BINOP_FIRST,
  MINUS,
  MULTIPLY,
  DIVIDE,
  INTDIV,
  MODULO,
  POWER,
  AND,
  XOR,
  OR,
  LSHIFT,
  RSHIFT,
  EQ,
  NE,
  LT,
  GT,
  LE,
  GE = BINOP_LAST,

  // String
  RE_MATCH = STRING_FIRST,

  // Reducers
  MEAN = REDUCER_FIRST,
  MIN,
  MAX,
  STDEV,
  FIRST,
  SUM,
  COUNT,
  COUNT0,
  MEDIAN = REDUCER_LAST,

  // Math: trigonometric
  SIN = MATH_FIRST,
  COS,
  TAN,
  ARCSIN,
  ARCCOS,
  ARCTAN,
  ARCTAN2,
  HYPOT,
  DEG2RAD,
  RAD2DEG,

  // Math: hyperbolic
  SINH,
  COSH,
  TANH,
  ARCSINH,
  ARCCOSH,
  ARCTANH,

  // Math: exponential/power
  CBRT,
  EXP,
  EXP2,
  EXPM1,
  LOG,
  LOG10,
  LOG1P,
  LOG2,
  LOGADDEXP,
  LOGADDEXP2,
  // POWER,
  SQRT,
  SQUARE,

  // Math: special
  ERF,
  ERFC,
  GAMMA,
  LGAMMA,

  // Math: floating-point
  // CEIL,
  COPYSIGN,
  FABS,
  // FLOOR,
  // FREXP: double->(double, int)
  // ISCLOSE: non-trivial signature
  LDEXP,
  NEXTAFTER,
  // RINT ?
  SIGN,
  SIGNBIT,
  // SPACING ?
  // TRUNC,

  // Math: misc
  CLIP,
  // DIVMOD (double,double)->(double,double)
  // MODF double->(double,double)
  MAXIMUM,
  MINIMUM,
  FMOD = MATH_LAST,
};



//------------------------------------------------------------------------------
// dt::expr::base_expr
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


// Initialize once
void init_expr();


py::oobj make_pyexpr(Op opcode, py::oobj arg);
py::oobj make_pyexpr(Op opcode, py::oobj arg1, py::oobj arg2);



}}  // namespace dt::expr


// hashing support
namespace std {
  template <> struct hash<dt::expr::Op> {
    std::size_t operator()(const dt::expr::Op& op) const {
      return static_cast<size_t>(op);
    }
  };
}

#endif
