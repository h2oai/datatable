#!/usr/bin/env python
#-------------------------------------------------------------------------------
# Copyright 2018-2023 H2O.ai
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
from .expr import Expr, OpCodes
from datatable.lib import core
from builtins import sum as _builtin_sum
from builtins import min as _builtin_min
from builtins import max as _builtin_max

__all__ = (
    "corr",
    "count",
    "cov",
    "first",
    "last",
    "max",
    "median",
    "min",
    "nunique",
    "sd",
    "sum",
    "countna",
)



def count(iterable=None):
    if isinstance(iterable, (Expr, core.FExpr)) or (iterable is None):
        return core.count(iterable)
    else:
        return _builtin_sum((x is not None) for x in iterable)


def nunique(iterable=None):
    return Expr(OpCodes.NUNIQUE, (iterable,))


def countna(iterable):
    return core.countna(iterable)


def first(iterable):
    if isinstance(iterable, (Expr, core.FExpr)):
        return Expr(OpCodes.FIRST, (iterable,))
    else:
        for x in iterable:
            return x


def last(iterable):
    if isinstance(iterable, (Expr, core.FExpr)):
        return Expr(OpCodes.LAST, (iterable,))
    else:
        try:
            for x in reversed(iterable):
                return x
        except TypeError:
            # Some iterators may not be reversible
            x = None
            for x in iterable:
                pass
            return x



def sd(expr):
    return Expr(OpCodes.STDEV, (expr,))


def median(expr):
    return Expr(OpCodes.MEDIAN, (expr,))


def cov(expr1, expr2):
    return Expr(OpCodes.COV, (expr1, expr2))


def corr(col1, col2):
    """
    Compute Pearson correlation coefficient between two columns.
    """
    return Expr(OpCodes.CORR, (col1, col2))


# noinspection PyShadowingBuiltins
def sum(iterable, start=0):
    if (not isinstance(iterable, dict)
        and (isinstance(iterable, core.FExpr)
        or (hasattr(iterable, "__getitem__") and isinstance(iterable[0], core.FExpr)))):
        return core.sum(iterable)
    elif isinstance(iterable, dict) and isinstance([*iterable.values()][0], core.FExpr):
        return core.sum(iterable)
    elif isinstance(iterable, core.Frame):
        return iterable.sum()
    else:
        return _builtin_sum(iterable, start)  

# noinspection PyShadowingBuiltins
def min(*args, **kwds):
    if (len(args) == 1
        and (not isinstance(args[0], dict)) and (isinstance(args[0], (Expr, core.FExpr))
        or (hasattr(args[0], "__getitem__") and isinstance(args[0][0], (Expr, core.FExpr))))):
        return core.min(args)
    elif len(args) == 1 and isinstance(args[0], dict) and isinstance([*args[0].values()][0], (Expr, core.FExpr)):
        return core.min(args)
    elif len(args) == 1 and isinstance(args[0], core.Frame):
        return args[0].min()
    else:
        return _builtin_min(*args, **kwds)


# noinspection PyShadowingBuiltins
def max(*args, **kwds):
    if (len(args) == 1 and (not isinstance(args[0], dict))
        and (isinstance(args[0], (Expr, core.FExpr))
        or (hasattr(args[0], "__getitem__") and isinstance(args[0][0], (Expr, core.FExpr))))):
        return core.max(args)
    elif len(args) == 1 and isinstance(args[0], dict) and isinstance([*args[0].values()][0], (Expr, core.FExpr)):
        return core.max(args)
    elif len(args) == 1 and isinstance(args[0], core.Frame):
        return args[0].max()
    else:
        return _builtin_max(*args, **kwds)

sum.__doc__ = _builtin_sum.__doc__
min.__doc__ = _builtin_min.__doc__
max.__doc__ = _builtin_max.__doc__
