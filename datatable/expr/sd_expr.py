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

    def is_reduce_expr(self, ee):
        return True

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


    def _value(self, key, inode):
        pf = inode.previous_function
        vavg, navg, vs, vcnt, vx = \
            pf.make_variable("runavg", "newavg", "runs", "runcnt", "x")
        vres = inode.make_variable("sd")
        i = inode.add_stack_variable(str(self))
        arg_isna = self.expr.isna(pf)
        arg_notna = self.expr.notna(pf)
        res_ctype = ctypes_map[self.stype]
        if self.skipna:
            pf.add_prologue_expr("double %s = 0;" % vavg)
            pf.add_prologue_expr("double %s;" % navg)
            pf.add_prologue_expr("double %s;" % vx)
            pf.add_prologue_expr("double %s = 0;" % vs)
            pf.add_prologue_expr("int64_t %s = 0;" % vcnt)
            pf.add_mainloop_expr("if (!%s) {" % arg_isna)
            pf.add_mainloop_expr("  %s++;" % vcnt)
            pf.add_mainloop_expr("  %s = %s;" % (vx, arg_notna))
            pf.add_mainloop_expr("  %s = %s + (%s - %s) / %s;"
                                 % (navg, vavg, vx, vavg, vcnt))
            pf.add_mainloop_expr("  if (%s > 1)" % vcnt)
            pf.add_mainloop_expr("    %s += (%s - %s)*(%s - %s);"
                                 % (vs, vx, vavg, vx, navg))
            pf.add_mainloop_expr("  %s = %s;" % (vavg, navg))
            pf.add_mainloop_expr("  ")
            pf.add_mainloop_expr("}")
            pf.add_epilogue_expr("%s %s = sqrt(%s / (%s - 1));"
                                 % (res_ctype, vres, vs, vcnt))
        pf.add_epilogue_expr("stack[%d].f8 = %s;" % (i, vres))
        inode.add_prologue_expr("%s %s = stack[%d].f8;"
                                % (res_ctype, vres, i))
        return vres


    def _isna(self, key, inode):
        if self.skipna:
            return "0"
        else:
            return "ISNA_F8(%s)" % self.value(inode)


    def _notna(self, key, inode):
        return self.value(inode)
