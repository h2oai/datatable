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
from datatable.graph.dtproxy import f
from datatable.lib import core
from cmath import exp

__all__ = ("exp",)

_builtin_exp = exp

def exp(x):
    if isinstance(x, BaseExpr):
        return UnaryOpExpr("exp", x)
    if isinstance(x, core.Frame):
        return x[:, {col: UnaryOpExpr("exp", f[col])
                     for col in x.names}]
    if x is None:
        return None
    try:
        return _builtin_exp(x)
    except OverflowError:
        return float("inf")
