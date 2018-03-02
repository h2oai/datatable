#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------

from .base_expr import BaseExpr
from .consts import ctypes_map, nas_map, reduce_opcodes
from ..utils.typechecks import Frame_t, is_type
from ..types import stype
from datatable.lib import core

__all__ = ("min", "max", "MinMaxReducer")

_builtin_min = min
_builtin_max = max


# noinspection PyShadowingBuiltins
def min(*args, **kwds):
    if len(args) == 1 and isinstance(args[0], BaseExpr):
        skipna = kwds.get("skipna", True)
        return MinMaxReducer(args[0], ismin=True, skipna=skipna)
    elif len(args) == 1 and is_type(args[0], Frame_t):
        return args[0].min()
    else:
        return _builtin_min(*args, **kwds)


# noinspection PyShadowingBuiltins
def max(*args, **kwds):
    if len(args) == 1 and isinstance(args[0], BaseExpr):
        skipna = kwds.get("skipna", True)
        return MinMaxReducer(args[0], ismin=False, skipna=skipna)
    elif len(args) == 1 and is_type(args[0], Frame_t):
        return args[0].max()
    else:
        return _builtin_max(*args, **kwds)



class MinMaxReducer(BaseExpr):

    def __init__(self, expr, ismin, skipna=True):
        super().__init__()
        self._arg = expr
        self._skipna = skipna
        self._op = "<" if ismin else ">"
        self._name = "min" if ismin else "max"

    def resolve(self):
        self._arg.resolve()
        self._stype = self._arg.stype


    def evaluate_eager(self):
        col = self._arg.evaluate_eager()
        opcode = reduce_opcodes[self._name]
        return core.expr_reduceop(opcode, col)


    def __str__(self):
        return "%s%d(%s)" % (self._name, self._skipna, self._arg)


    def _value(self, key, inode):
        pf = inode.previous_function
        curr = pf.make_variable("run%s" % self._name)
        curr_isna = pf.make_variable("run%s_isna" % self._name)
        res = inode.make_variable(self._name)
        i = inode.add_stack_variable(str(self))
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
        inode.add_prologue_expr("%s %s = stack[%d].%s;"
                                % (ctype, res, i, self.stype[:2]))
        return res


    def _isna(self, key, inode):
        if self.stype == stype.float64:
            return "ISNA_F8(%s)" % self.value(inode)
        elif self.stype == stype.float32:
            return "ISNA_F4(%s)" % self.value(inode)
        else:
            return "(%s == %s)" % (self.value(inode), nas_map[self.stype])


    def _notna(self, key, inode):
        return self.value(inode)
