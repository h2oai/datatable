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
#include "groupby.h"


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
  "expr_column(dt, i, rowindex)\n\n"
  "Retrieve column `i` from the DataTable `dt`, replacing its rowindex with\n"
  "the provided one and then materializing.\n",
  dt_EXPR_PY_EXPR_CC)

DECLARE_FUNCTION(
  expr_reduceop,
  "expr_reduceop(op, col, groupby)\n\n"
  "Compute a reduction over the provided column, using the provided Groupby\n"
  "object.\n",
  dt_EXPR_PY_EXPR_CC)

DECLARE_FUNCTION(
  expr_unaryop,
  "expr_unaryop(op, col)\n\n"
  "Apply unary operator `op` to the column `col` and return the resulting \n"
  "column.\n",
  dt_EXPR_PY_EXPR_CC)

DECLARE_FUNCTION(
  expr_count,
  "expr_count(dt, groupby)\n\n"
  "Compute number of rows in each group if the Groupby object is provided, otherwise simply\n"
  "return number of rows in dataframe.\n",
  dt_EXPR_PY_EXPR_CC)


namespace expr {

typedef void (*mapperfn)(int64_t row0, int64_t row1, void** params);
typedef void (*gmapperfn)(const int32_t* groups, int32_t grp, void** params);

Column* unaryop(int opcode, Column* arg);
Column* binaryop(size_t opcode, Column* lhs, Column* rhs);
Column* reduceop(int opcode, Column* arg, const Groupby& groupby);
Column* reduce_first(const Column* col, const Groupby& groupby);

};

#endif
