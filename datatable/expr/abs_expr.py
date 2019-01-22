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
from datatable.lib import core

__all__ = ("abs", )

_builtin_abs = abs

def abs(x):
    if isinstance(x, BaseExpr):
        return UnaryOpExpr("abs", x)
    if isinstance(x, core.Frame):
        from datatable.graph.dtproxy import f
        return x[:, {col: UnaryOpExpr("abs", f[col])
                     for col in x.names}]
    if x is None:
        return None
    return _builtin_abs(x)
