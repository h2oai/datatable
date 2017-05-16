#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .base_expr import BaseExpr
from .consts import ctypes_map, nas_map

__all__ = ("min", "max", "MinMaxReducer")

_builtin_min = min
_builtin_max = max


# noinspection PyShadowingBuiltins
def min(*args, **kwds):
    if len(args) == 1 and isinstance(args[0], BaseExpr):
        skipna = kwds.get("skipna", True)
        return MinMaxReducer(args[0], ismin=True, skipna=skipna)
    else:
        return _builtin_min(*args, **kwds)


# noinspection PyShadowingBuiltins
def max(*args, **kwds):
    if len(args) == 1 and isinstance(args[0], BaseExpr):
        skipna = kwds.get("skipna", True)
        return MinMaxReducer(args[0], ismin=False, skipna=skipna)
    else:
        return _builtin_max(*args, **kwds)



class MinMaxReducer(BaseExpr):

    def __init__(self, expr, ismin, skipna=True):
        super().__init__()
        self._arg = expr
        self._skipna = skipna
        self._op = "<" if ismin else ">"
        self._name = "min" if ismin else "max"
        self.stype = expr.stype


    def __str__(self):
        return "%s%d(%s)" % (self._name, self._skipna, self._arg)


    def _value(self, block):
        pf = block.previous_function
        curr = pf.make_variable("run%s" % self._name)
        curr_isna = pf.make_variable("run%s_isna" % self._name)
        res = block.make_variable(self._name)
        i = block.add_stack_variable(str(self))
        arg_isna = self._arg.isna(pf)
        arg_value = self._arg.value(pf)
        ctype = ctypes_map[self.stype]
        na = nas_map[self.stype]
        pf.add_prologue_expr("%s %s;" % (ctype, curr))
        pf.add_prologue_expr("int %s = 1;" % curr_isna)
        if self._skipna:
            pf.add_mainloop_expr(
                "if (!%s && (%s || %s %s %s)) {"
                % (arg_isna, curr_isna, arg_value, self._op, curr))
        else:
            pf.add_mainloop_expr(
                "if (%s || (!%s && %s %s %s)) {"
                % (curr_isna, arg_isna, arg_value, self._op, curr))
        pf.add_mainloop_expr("  %s = 0;" % curr_isna)
        pf.add_mainloop_expr("  %s = %s;" % (curr, arg_value))
        pf.add_mainloop_expr("}")
        pf.add_epilogue_expr("stack[%d].%s = %s? %s : %s;"
                             % (i, self.stype[:2], curr_isna, na, curr))
        block.add_prologue_expr("%s %s = stack[%d].%s;"
                                % (ctype, res, i, self.stype[:2]))
        return res


    def _isna(self, block):
        if self.stype == "f8r":
            return "ISNA_F8(%s)" % self.value(block)
        elif self.stype == "f4r":
            return "ISNA_F4(%s)" % self.value(block)
        else:
            return "(%s == %s)" % (self.value(block), nas_map[self.stype])


    def _notna(self, block):
        return self.value(block)
