#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .base_expr import BaseExpr



class UnaryOpExpr(BaseExpr):

    def __init__(self, op, arg):
        super().__init__()
        self.op = op
        self.arg = arg
        self.stype = arg.stype


    def _isna(self, block):
        return self.arg.isna(block)


    def _notna(self, block):
        return "(%s %s)" % (self.op, self.arg.notna(block))


    def __str__(self):
        return "(%s %s)" % (self.op, self.arg)
