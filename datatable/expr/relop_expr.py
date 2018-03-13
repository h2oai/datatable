#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------

from .base_expr import BaseExpr
from .binary_expr import binary_op_codes
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

    def resolve(self):
        self._lhs.resolve()
        self._rhs.resolve()
        self._stype = stype.bool8


    #---------------------------------------------------------------------------
    # LLVM evaluation
    #---------------------------------------------------------------------------

    def _isna(self, key, inode):
        return False


    def _notna(self, key, inode):
        lhs_notna = self._lhs.notna(inode)
        rhs_notna = self._rhs.notna(inode)
        lhs_isna = self._lhs.isna(inode)
        rhs_isna = self._rhs.isna(inode)
        if isinstance(lhs_isna, bool):
            lhs_isna = int(lhs_isna)
        if isinstance(rhs_isna, bool):
            rhs_isna = int(rhs_isna)
        if lhs_notna == "None":
            lhs_notna = "0"
        if rhs_notna == "None":
            rhs_notna = "0"

        if self._op == "!=":
            teststr = ("({lhsna} || {rhsna} || ({lhs} != {rhs})) && "
                       "!({lhsna} && {rhsna})")
        else:
            teststr = "(!{lhsna} && !{rhsna} && ({lhs} {op} {rhs}))"
            if self._op[-1] == "=":
                teststr += " || ({lhsna} && {rhsna})"
        return teststr.format(lhsna=lhs_isna, rhsna=rhs_isna,
                              lhs=lhs_notna, rhs=rhs_notna, op=self._op)



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
