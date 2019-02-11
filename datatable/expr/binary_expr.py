#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------

from .base_expr import BaseExpr
from .literal_expr import LiteralExpr
from .consts import ops_rules, division_ops, baseexpr_opcodes
from datatable.utils.typechecks import TTypeError, TValueError
from datatable.lib import core


class BinaryOpExpr(BaseExpr):
    __slots__ = ["_op", "_lhs", "_rhs"]

    def __init__(self, lhs, op, rhs):
        super().__init__()
        if not isinstance(lhs, BaseExpr):
            lhs = LiteralExpr(lhs)
        if not isinstance(rhs, BaseExpr):
            rhs = LiteralExpr(rhs)
        self._op = op    # type: str
        self._lhs = lhs  # type: BaseExpr
        self._rhs = rhs  # type: BaseExpr

    def resolve(self):
        self._lhs.resolve()
        self._rhs.resolve()
        triple = (self._op, self._lhs.stype, self._rhs.stype)
        self._stype = ops_rules.get(triple, None)
        if self._stype is None:
            raise TTypeError("Operation %s not allowed on operands of types "
                             "%s and %s"
                             % (self._op, self._lhs.stype, self._rhs.stype))
        # if self._stype in stype_decimal:
        #     self.scale = max(self.lhs.scale, self.rhs.scale)


    def __str__(self):
        return "(%s %s %s)" % (self._lhs, self._op, self._rhs)


    def _core(self):
        return core.base_expr(baseexpr_opcodes["binop"],
                              binary_op_codes[self._op],
                              self._lhs._core(),
                              self._rhs._core())

    #---------------------------------------------------------------------------
    # Eager evaluation
    #---------------------------------------------------------------------------

    def evaluate_eager(self, ee):
        lhs = self._lhs.evaluate_eager(ee)
        rhs = self._rhs.evaluate_eager(ee)
        nl = lhs.nrows
        nr = rhs.nrows
        if nl == nr or nl == 1 or nr == 1:
            opcode = binary_op_codes[self._op]
            return core.expr_binaryop(opcode, lhs, rhs)
        else:
            raise TValueError("Cannot apply op '%s' on incompatible columns "
                              "of sizes %d and %d" % (self._op, nl, nr))


#-------------------------------------------------------------------------------
# Should be in sync with enum in "expr/binaryop.cc"
binary_op_codes = {
    "+": 1, "-": 2, "*": 3, "/": 4, "//": 5, "**": 6, "%": 7,
    "&": 8, "|": 9, "<<": 10, ">>": 11,
    "==": 12, "!=": 13, ">": 14, "<": 15, ">=": 16, "<=": 17
}
