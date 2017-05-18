#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .base_expr import BaseExpr
from .consts import ops_rules, decimal_stypes, ctypes_map, nas_map


def mean(expr, skipna=True):
    return MeanReducer(expr, skipna)


class MeanReducer(BaseExpr):

    def __init__(self, expr, skipna=True):
        super().__init__()
        self.expr = expr
        self.skipna = skipna
        self._stype = ops_rules.get(("mean", expr.stype), None)
        if self._stype is None:
            raise ValueError(
                "Cannot compute mean of a variable of type %s" % expr.stype)
        self.scale = 0
        if self._stype in decimal_stypes:
            self.scale = expr.scale


    def __str__(self):
        return "mean%d(%s)" % (self.skipna, self.expr)


    def _value(self, block):
        pf = block.previous_function
        vsum, vcnt, vres = pf.make_variable("sum", "cnt", "res")
        visna = vsum + "_isna"
        v = block.make_variable("mean")
        i = block.add_stack_variable(str(self))
        arg_isna = self.expr.isna(pf)
        arg_notna = self.expr.notna(pf)
        res_ctype = ctypes_map[self.stype]
        if self.skipna:
            pf.add_prologue_expr("long double %s = 0;" % vsum)
            pf.add_prologue_expr("int64_t %s = 0;" % vcnt)
            pf.add_mainloop_expr("%s += %s? 0 : %s;"
                                 % (vsum, arg_isna, arg_notna))
            pf.add_mainloop_expr("%s += %s;" % (vcnt, arg_isna))
            pf.add_epilogue_expr("%s %s = %s / %s;"
                                 % (res_ctype, vres, vsum, vcnt))
        else:
            pf.add_prologue_expr("long double %s = 0;" % vsum)
            pf.add_prologue_expr("int %s = 0;" % visna)
            pf.add_mainloop_expr("%s |= %s;" % (visna, arg_isna))
            pf.add_mainloop_expr("%s += %s? 0 : (%s);"
                                 % (vsum, visna, arg_notna))
            pf.add_epilogue_expr("%s %s = %s? %s : (%s / nrows);"
                                 % (res_ctype, vres, visna,
                                    nas_map[self.stype], vsum))
        pf.add_epilogue_expr("stack[%d].f8 = %s;" % (i, vres))
        block.add_prologue_expr("%s %s = stack[%d].f8;"
                                % (res_ctype, v, i))
        return v


    def _isna(self, block):
        if self.skipna:
            return "0"
        else:
            return "ISNA_F8(%s)" % self.value(block)


    def _notna(self, block):
        return self.value(block)
