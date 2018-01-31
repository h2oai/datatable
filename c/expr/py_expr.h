//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_EXPR_PY_EXPR_h
#define dt_EXPR_PY_EXPR_h
#include "py_utils.h"
#include "column.h"


DECLARE_FUNCTION(
  expr_binaryop,
  "expr_binaryop(op, lhs, rhs)\n\n"
  "\n",
  dt_EXPR_PY_EXPR_CC)

DECLARE_FUNCTION(
  expr_cast,
  "expr_cast(col, stype)\n\n"
  "Convert column into new stype.\n",
  dt_EXPR_PY_EXPR_CC)

DECLARE_FUNCTION(
  expr_column,
  "expr_column(dt, index)\n\n"
  "Retrieve column `index` from the DataTable `dt`. If the column has \n"
  "associated rowindex, it will be materialized first.\n",
  dt_EXPR_PY_EXPR_CC)

DECLARE_FUNCTION(
  expr_reduceop,
  "expr_reduceop(op, col)\n\n"
  "Compute a reduction over the provided column.\n",
  dt_EXPR_PY_EXPR_CC)

DECLARE_FUNCTION(
  expr_unaryop,
  "expr_unaryop(op, col)\n\n"
  "Apply unary operator `op` to the column `col` and return the resulting \n"
  "column.\n",
  dt_EXPR_PY_EXPR_CC)


namespace expr {

typedef void (*mapperfn)(int64_t row0, int64_t row1, void** params);

Column* unaryop(int opcode, Column* arg);
Column* binaryop(int opcode, Column* lhs, Column* rhs);
Column* reduceop(int opcode, Column* arg);

};

#endif
