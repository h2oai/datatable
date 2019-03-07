//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_EXPR_PY_EXPR_h
#define dt_EXPR_PY_EXPR_h
#include "expr/base_expr.h"
#include "column.h"
#include "groupby.h"
#include "py_utils.h"


namespace expr {

typedef void (*mapperfn)(int64_t row0, int64_t row1, void** params);
typedef void (*gmapperfn)(const int32_t* groups, int32_t grp, void** params);

Column* unaryop(dt::unop opcode, Column* arg);
Column* binaryop(size_t opcode, Column* lhs, Column* rhs);
Column* reduceop(int opcode, Column* arg, const Groupby& groupby);
Column* reduce_first(const Column* col, const Groupby& groupby);

};

#endif
