#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------

from .base_expr import BaseExpr
from .binary_expr import binary_op_codes
from .consts import baseexpr_opcodes
from .literal_expr import LiteralExpr
from ..types import stype
from datatable.utils.typechecks import TValueError
from datatable.lib import core



class RelationalOpExpr(BaseExpr):
    __slots__ = ["_op", "_lhs", "_rhs"]

    def __init__(self, lhs, op, rhs):
        super().__init__()
        if not isinstance(rhs, BaseExpr):
            rhs = LiteralExpr(rhs)
        # `op` is one of "==", "!=", ">", "<", ">=", "<="
        self._op = op
        self._lhs = lhs
        self._rhs = rhs

    def __str__(self):
        return "(%s %s %s)" % (self._lhs, self._op, self._rhs)


    def _core(self):
        return core.base_expr(baseexpr_opcodes["binop"],
                              binary_op_codes[self._op],
                              self._lhs._core(),
                              self._rhs._core())
