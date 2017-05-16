#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .base_expr import BaseExpr
from .literal_expr import LiteralNode
from .consts import ops_rules, decimal_stypes, division_ops



class BinaryOpExpr(BaseExpr):

    def __init__(self, lhs, op, rhs):
        super().__init__()
        if not isinstance(lhs, BaseExpr):
            lhs = LiteralNode(lhs)
        if not isinstance(rhs, BaseExpr):
            rhs = LiteralNode(rhs)
        self.op = op
        self.lhs = lhs
        self.rhs = rhs
        self.stype = ops_rules.get((self.op, lhs.stype, rhs.stype), None)
        if self.stype is None:
            raise TypeError("Operation %s not allowed on operands of types "
                            "%s and %s" % (self.op, lhs.stype, rhs.stype))
        if self.stype in decimal_stypes:
            self.scale = max(lhs.scale, rhs.scale)


    def _isna(self, block):
        lhs_isna = self.lhs.isna(block)
        rhs_isna = self.rhs.isna(block)
        if lhs_isna is True or rhs_isna is True:
            return True
        conditions = []
        if lhs_isna is not False:
            conditions.append(lhs_isna)
        if rhs_isna is not False:
            conditions.append(rhs_isna)
        if self.op in division_ops:
            conditions.append("(%s == 0)" % self.rhs.notna(block))
        if not conditions:
            return False
        isna = block.make_variable("isna")
        expr = "int %s = %s;" % (isna, " | ".join(conditions))
        block.add_mainloop_expr(expr)
        return isna


    def _notna(self, block):
        lhs = self.lhs.notna(block)
        rhs = self.rhs.notna(block)
        return "(%s %s %s)" % (lhs, self.op, rhs)


    def __str__(self):
        return "(%s %s %s)" % (self.lhs, self.op, self.rhs)
