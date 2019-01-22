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
from .base_expr import BaseExpr
from .unary_expr import UnaryOpExpr
from .dtproxy import f
from datatable.lib import core
import math

__all__ = ("exp", "log", "log10")


def exp(x):
    if isinstance(x, BaseExpr):
        return UnaryOpExpr("exp", x)
    if isinstance(x, core.Frame):
        return x[:, {col: UnaryOpExpr("exp", f[col])
                     for col in x.names}]
    if x is None:
        return None
    try:
        return math.exp(x)
    except OverflowError:
        return math.inf


def log(x):
    if isinstance(x, BaseExpr):
        return UnaryOpExpr("log", x)
    if isinstance(x, core.Frame):
        return x[:, {col: UnaryOpExpr("log", f[col])
                     for col in x.names}]
    if x is None or x < 0:
        return None
    elif x == 0:
        return -math.inf
    else:
        return math.log(x)


def log10(x):
    if isinstance(x, BaseExpr):
        return UnaryOpExpr("log10", x)
    if isinstance(x, core.Frame):
        return x[:, {col: UnaryOpExpr("log10", f[col])
                     for col in x.names}]
    if x is None or x < 0:
        return None
    elif x == 0:
        return -math.inf
    else:
        return math.log10(x)
