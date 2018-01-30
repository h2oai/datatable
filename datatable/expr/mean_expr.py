#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .base_expr import BaseExpr
from .consts import ops_rules, ctypes_map, nas_map, reduce_opcodes
from datatable.lib import core


def mean(expr, skipna=True):
    return MeanReducer(expr, skipna)


class MeanReducer(BaseExpr):
    __slots__ = ["expr", "skipna"]

    def __init__(self, expr, skipna=True):
        super().__init__()
        self.expr = expr
        self.skipna = skipna


    def resolve(self):
        self.expr.resolve()
        expr_stype = self.expr.stype
        self._stype = ops_rules.get(("mean", expr_stype), None)
        if self._stype is None:
            raise ValueError(
                "Cannot compute mean of a variable of type %s" % expr_stype)


    def evaluate_eager(self):
        col = self.expr.evaluate_eager()
        opcode = reduce_opcodes["mean"]
        return core.expr_reduceop(opcode, col)


    def __str__(self):
        return "mean%d(%s)" % (self.skipna, self.expr)


    def _value(self, key, inode):
        pf = inode.previous_function
        vsum, vcnt, vres = pf.make_variable("sum", "cnt", "res")
        visna = vsum + "_isna"
        v = inode.make_variable("mean")
        i = inode.add_stack_variable(str(self))
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
        inode.add_prologue_expr("%s %s = stack[%d].f8;"
                                % (res_ctype, v, i))
        return v


    def _isna(self, key, inode):
        if self.skipna:
            return "0"
        else:
            return "ISNA_F8(%s)" % self.value(inode)


    def _notna(self, key, inode):
        return self.value(inode)
