#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .base_expr import BaseExpr
from .literal_expr import LiteralExpr
from .consts import ops_rules, division_ops
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


    #---------------------------------------------------------------------------
    # LLVM evaluation
    #---------------------------------------------------------------------------

    def _isna(self, key, inode):
        lhs_isna = self._lhs.isna(inode)
        rhs_isna = self._rhs.isna(inode)
        if lhs_isna is True or rhs_isna is True:
            return True
        conditions = []
        if lhs_isna is not False:
            conditions.append(lhs_isna)
        if rhs_isna is not False:
            conditions.append(rhs_isna)
        if self._op in division_ops:
            conditions.append("(%s == 0)" % self._rhs.notna(inode))
        if not conditions:
            return False
        isna = inode.make_keyvar(key, "isna")
        expr = "int %s = %s;" % (isna, " | ".join(conditions))
        inode.addto_mainloop(expr)
        return isna


    def _notna(self, key, inode):
        lhs = self._lhs.notna(inode)
        rhs = self._rhs.notna(inode)
        return "(%s %s %s)" % (lhs, self._op, rhs)


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



# Should be in sync with enum in "expr/binaryop.cc"
_binary_op_codes = {
    "+": 1, "-": 2, "*": 3, "/": 4, "//": 5, "**": 6, "%": 7,
    "&&": 8, "||": 9, "<<": 10, ">>": 11
}
