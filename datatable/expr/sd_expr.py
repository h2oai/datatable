#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#-------------------------------------------------------------------------------
from .base_expr import BaseExpr
from .consts import ops_rules, ctypes_map, reduce_opcodes, baseexpr_opcodes
from datatable.lib import core


def sd(expr, skipna=True):
    return StdevReducer(expr, skipna)



class StdevReducer(BaseExpr):

    def __init__(self, expr, skipna=True):
        super().__init__()
        self.expr = expr
        self.skipna = skipna

    def resolve(self):
        self.expr.resolve()
        self._stype = ops_rules.get(("sd", self.expr.stype), None)
        if self.stype is None:
            raise ValueError(
                "Cannot compute standard deviation of a variable of type %s"
                % self.expr.stype)

    def evaluate_eager(self, ee):
        col = self.expr.evaluate_eager(ee)
        opcode = reduce_opcodes["stdev"]
        return core.expr_reduceop(opcode, col, ee.groupby)


    def __str__(self):
        return "sd%d(%s)" % (self.skipna, self.expr)


    def _core(self):
        return core.base_expr(baseexpr_opcodes["unreduce"],
                              reduce_opcodes["stdev"],
                              self.expr._core())
