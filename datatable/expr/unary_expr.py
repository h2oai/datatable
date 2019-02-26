#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------

from .base_expr import BaseExpr
from ..types import stype
from datatable.utils.typechecks import TTypeError
from datatable.lib import core

# See c/expr/base_expr.h
BASEEXPR_OPCODE_UNOP = 4


class UnaryOpExpr(BaseExpr):
    __slots__ = ["_op", "_arg"]

    def __init__(self, op, arg):
        super().__init__()
        self._op = op
        self._arg = arg


    def __str__(self):
        return "(%s %s)" % (self._op, self._arg)


    def _core(self):
        return core.base_expr(BASEEXPR_OPCODE_UNOP,
                              unary_op_codes[self._op],
                              self._arg._core())


# Synchronize with OpCode in c/expr/unaryop.cc
unary_op_codes = {
    "isna": 1,
    "-": 2,
    "+": 3,
    "~": 4,
    "!": 4,  # same as '~'
    "abs": 5,
    "exp": 6,
    "log": 7,
    "log10": 8,
    "len": 9,
}



#-------------------------------------------------------------------------------
# Exported functions
#-------------------------------------------------------------------------------

def isna(x):
    if isinstance(x, BaseExpr):
        return UnaryOpExpr("isna", x)
    if isinstance(x, core.Frame):
        if x.ncols != 1:
            raise TTypeError("Frame must have a single column")
        return x[:, isna(f[0])]
    return (x is None) or (isinstance(x, float) and math.isnan(x))
