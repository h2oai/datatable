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
from .consts import ctypes_map, nas_map, reduce_opcodes, baseexpr_opcodes
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


min.__doc__ = _builtin_min.__doc__
max.__doc__ = _builtin_max.__doc__


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


    def __str__(self):
        return "%s%d(%s)" % (self._name, self._skipna, self._arg)


    def _core(self):
        return core.base_expr(baseexpr_opcodes["unreduce"],
                              reduce_opcodes[self._name],
                              self._arg._core())
