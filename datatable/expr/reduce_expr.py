#!/usr/bin/env python
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
from datatable.frame import Frame
from datatable.lib import core

_builtin_sum = sum
_builtin_min = min
_builtin_max = max

# See "c/expr/base_expr.h"
BASEEXPR_OPCODE_UNARY_REDUCE = 6
BASEEXPR_OPCODE_NULLARY_REDUCE = 7



#-------------------------------------------------------------------------------
# Exported functions
#-------------------------------------------------------------------------------

def count(iterable=None):
    if isinstance(iterable, BaseExpr):
        return ReduceExpr("count", iterable)
    elif iterable is None:
        return CountExpr()
    else:
        return _builtin_sum((x is not None) for x in iterable)


def first(iterable):
    if isinstance(iterable, BaseExpr):
        return ReduceExpr("first", iterable)
    else:
        for x in iterable:
            return x


def mean(expr):
    return ReduceExpr("mean", expr)


def sd(expr):
    return ReduceExpr("stdev", expr)


def median(expr):
    return ReduceExpr("median", expr)


# noinspection PyShadowingBuiltins
def sum(iterable, start=0):
    if isinstance(iterable, BaseExpr):
        return ReduceExpr("sum", iterable)
    else:
        return _builtin_sum(iterable, start)


# noinspection PyShadowingBuiltins
def min(*args, **kwds):
    if len(args) == 1 and isinstance(args[0], BaseExpr):
        return ReduceExpr("min", args[0])
    elif len(args) == 1 and isinstance(args[0], Frame):
        return args[0].min()
    else:
        return _builtin_min(*args, **kwds)


# noinspection PyShadowingBuiltins
def max(*args, **kwds):
    if len(args) == 1 and isinstance(args[0], BaseExpr):
        return ReduceExpr("max", args[0])
    elif len(args) == 1 and isinstance(args[0], Frame):
        return args[0].max()
    else:
        return _builtin_max(*args, **kwds)


sum.__doc__ = _builtin_sum.__doc__
min.__doc__ = _builtin_min.__doc__
max.__doc__ = _builtin_max.__doc__




class CountExpr(BaseExpr):

    def __str__(self):
        return "count()"

    def _core(self):
        return core.base_expr(BASEEXPR_OPCODE_NULLARY_REDUCE, 0)



class ReduceExpr(BaseExpr):
    __slots__ = ["_op", "_expr"]

    def __init__(self, op, expr):
        self._op = op
        self._expr = expr


    def __str__(self):
        return "%s(%s)" % (self._op, self._expr)


    def _core(self):
        return core.base_expr(BASEEXPR_OPCODE_UNARY_REDUCE,
                              reduce_opcodes[self._op],
                              self._expr._core())


# Synchronize with c/expr/reduceop.cc
reduce_opcodes = {
    "mean": 1,
    "min": 2,
    "max": 3,
    "stdev": 4,
    "first": 5,
    "sum": 6,
    "count": 7,
    "median": 8,
}
