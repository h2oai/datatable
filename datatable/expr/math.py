#!/usr/bin/env python3
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#-------------------------------------------------------------------------------
from .expr import f, Expr, OpCodes
from datatable.lib import core
from builtins import abs as _builtin_abs
import math

__all__ = ("abs", "exp", "log", "log10", "isna")


def abs(x):
    if isinstance(x, Expr):
        return Expr(OpCodes.ABS, x)
    if isinstance(x, core.Frame):
        return x[:, {x.names[i]: Expr(OpCodes.ABS, f[i])
                     for i in range(x.ncols)}]
    if x is None:
        return None
    return _builtin_abs(x)


def isna(x):
    if isinstance(x, Expr):
        return Expr(OpCodes.ISNA, x)
    if isinstance(x, core.Frame):
        return x[:, {x.names[i]: Expr(OpCodes.EXP, f[i])
                     for i in range(x.ncols)}]
    return (x is None) or (isinstance(x, float) and math.isnan(x))



def exp(x):
    if isinstance(x, Expr):
        return Expr(OpCodes.EXP, x)
    if isinstance(x, core.Frame):
        return x[:, {x.names[i]: Expr(OpCodes.EXP, f[i])
                     for i in range(x.ncols)}]
    if x is None:
        return None
    try:
        return math.exp(x)
    except OverflowError:
        return math.inf


def log(x):
    if isinstance(x, Expr):
        return Expr(OpCodes.LOGE, x)
    if isinstance(x, core.Frame):
        return x[:, {x.names[i]: Expr(OpCodes.LOGE, f[i])
                     for i in range(x.ncols)}]
    if x is None or x < 0:
        return None
    elif x == 0:
        return -math.inf
    else:
        return math.log(x)


def log10(x):
    if isinstance(x, Expr):
        return Expr(OpCodes.LOG10, x)
    if isinstance(x, core.Frame):
        return x[:, {x.names[i]: Expr(OpCodes.LOG10, f[i])
                     for i in range(x.ncols)}]
    if x is None or x < 0:
        return None
    elif x == 0:
        return -math.inf
    else:
        return math.log10(x)
