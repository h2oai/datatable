#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .base_expr import BaseExpr
from .literal_expr import LiteralNode



class RelationalOpExpr(BaseExpr):

    def __init__(self, lhs, op, rhs):
        super().__init__()
        if not isinstance(rhs, BaseExpr):
            rhs = LiteralNode(rhs)
        self.op = op
        self.lhs = lhs
        self.rhs = rhs
        self.stype = "i1b"


    def _isna(self, block):
        return False


    def _notna(self, block):
        lhs_isna = self.lhs.isna(block)
        rhs_isna = self.rhs.isna(block)
        if lhs_isna is True or rhs_isna is True:
            return "0"
        conditions = []
        if lhs_isna is not False:
            conditions.append("!%s" % lhs_isna)
        if rhs_isna is not False:
            conditions.append("!%s" % rhs_isna)
        lhs_value = self.lhs.notna(block)
        rhs_value = self.rhs.notna(block)
        conditions.append("(%s %s %s)" % (lhs_value, self.op, rhs_value))
        return "(%s)" % " && ".join(conditions)


    def __str__(self):
        return "(%s %s %s)" % (self.lhs, self.op, self.rhs)
