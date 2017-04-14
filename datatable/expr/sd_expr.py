#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from ._expr import ExprNode
from .consts import ops_rules, decimal_stypes, ctypes_map, nas_map


def sd(expr, skipna=True):
    return StdevReducer(expr, skipna)


class StdevReducer(ExprNode):

    def __init__(self, expr, skipna=True):
        super().__init__()
        self.expr = expr
        self.skipna = skipna
        self.stype = ops_rules.get(("sd", expr.stype), None)
        if self.stype is None:
            raise ValueError(
                "Cannot compute standard deviation of a variable of type %s"
                % expr.stype)
        self.scale = 0
        if self.stype in decimal_stypes:
            self.scale = expr.scale


    def __str__(self):
        return "sd%d(%s)" % (self.skipna, self.expr)


    def _value(self, block):
        pf = block.previous_function
        vavg, navg, vs, vcnt, vx = \
            pf.make_variable("runavg", "newavg", "runs", "runcnt", "x")
        vres = block.make_variable("sd")
        i = block.add_stack_variable(str(self))
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
        block.add_prologue_expr("%s %s = stack[%d].f8;"
                                % (res_ctype, vres, i))
        return vres


    def _isna(self, block):
        if self.skipna:
            return "0"
        else:
            return "ISNA_F64(%s)" % self.value(block)


    def _notna(self, block):
        return self.value(block)
