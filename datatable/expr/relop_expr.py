#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .base_expr import BaseExpr
from .binary_expr import _binary_op_codes
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
        lhs_isna = self._lhs.isna(inode)
        rhs_isna = self._rhs.isna(inode)
        if lhs_isna is True or rhs_isna is True:
            return "0"
        conditions = []
        if lhs_isna is not False:
            conditions.append("!" + lhs_isna)
        if rhs_isna is not False:
            conditions.append("!" + rhs_isna)
        conditions.append("({lhs} {op} {rhs})"
                          .format(lhs=self._lhs.notna(inode),
                                  rhs=self._rhs.notna(inode),
                                  op=self._op))
        return "(" + " && ".join(conditions) + ")"


    #---------------------------------------------------------------------------
    # Eager evaluation
    #---------------------------------------------------------------------------

    def evaluate(self):
        lhs = self._lhs.evaluate()
        rhs = self._rhs.evaluate()
        nl = lhs.nrows
        nr = rhs.nrows
        if nl == nr or nl == 1 or nr == 1:
            opcode = _binary_op_codes[self._op]
            return core.expr_binaryop(opcode, lhs, rhs)
        else:
            raise TValueError("Cannot apply op '%s' on incompatible columns "
                              "of sizes %d and %d" % (self._op, nl, nr))
