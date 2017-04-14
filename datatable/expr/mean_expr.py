#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from ._expr import ExprNode
from .consts import ops_rules, decimal_stypes, ctypes_map, nas_map


def mean(expr, skipna=True):
    return MeanReducer(expr, skipna)


class MeanReducer(ExprNode):

    def __init__(self, expr, skipna=True):
        super().__init__()
        self.expr = expr
        self.skipna = skipna
        self.stype = ops_rules.get(("mean", expr.stype), None)
        if self.stype is None:
            raise ValueError(
                "Cannot compute mean of a variable of type %s" % expr.stype)
        self.scale = 0
        if self.stype in decimal_stypes:
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
            pf.add_prologue_expr(f"long double {vsum} = 0;")
            pf.add_prologue_expr(f"int64_t {vcnt} = 0;")
            pf.add_mainloop_expr(f"{vsum} += {arg_isna}? 0 : {arg_notna};")
            pf.add_mainloop_expr(f"{vcnt} += {arg_isna};")
            pf.add_epilogue_expr(f"{res_ctype} {vres} = {vsum} / {vcnt};")
        else:
            pf.add_prologue_expr(f"long double {vsum} = 0;")
            pf.add_prologue_expr(f"int {visna} = 0;")
            pf.add_mainloop_expr(f"{visna} |= {arg_isna};")
            pf.add_mainloop_expr(f"{vsum} += {visna}? 0 : ({arg_notna});")
            pf.add_epilogue_expr(f"{res_ctype} {vres} = {visna}? "
                                 f"{nas_map[self.stype]} : ({vsum} / nrows);")
        pf.add_epilogue_expr(f"stack[{i}].f8 = {vres};")
        block.add_prologue_expr(f"{res_ctype} {v} = stack[{i}].f8;")
        return v

    def _isna(self, context):
        return "0"

    def _notna(self, block):
        return self._value(block)
