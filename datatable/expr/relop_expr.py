#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .base_expr import BaseExpr
from .literal_expr import LiteralNode



class RelationalOpExpr(BaseExpr):

    def __init__(self, lhs, op, rhs):
        super().__init__()
        if not isinstance(rhs, BaseExpr):
            rhs = LiteralNode(rhs)
        # `op` is one of "==", "!=", ">", "<", ">=", "<="
        self._op = op
        self._lhs = lhs
        self._rhs = rhs
        self._stype = "i1b"


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


    def __str__(self):
        return "(%s %s %s)" % (self._lhs, self._op, self._rhs)
