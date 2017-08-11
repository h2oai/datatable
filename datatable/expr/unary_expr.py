#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .base_expr import BaseExpr
from .consts import unary_ops_rules



class UnaryOpExpr(BaseExpr):

    def __init__(self, op, arg):
        super().__init__()
        self.op = op
        self.arg = arg
        self._stype = unary_ops_rules.get((self.op, arg.stype), None)
        if self._stype is None:
            raise TypeError("Operation %s not allowed on operands of type %s"
                            % (self.op, arg.stype))
        if self.op == "~" and self._stype == "i1b":
            self.op = "!"



    def _isna(self, key, block):
        return self.arg.isna(block)


    def _notna(self, key, block):
        return "(%s %s)" % (self.op, self.arg.notna(block))


    def __str__(self):
        return "(%s %s)" % (self.op, self.arg)
