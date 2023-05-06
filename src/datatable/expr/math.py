#!/usr/bin/env python3
# -------------------------------------------------------------------------------
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
# -------------------------------------------------------------------------------
from datatable.lib import core
import math

__all__ = ("abs", "exp", "log", "log10", "isna")


def isna(iterable):
    if isinstance(iterable, core.FExpr):
        return core.isna(iterable)
    return (iterable is None) or (iterable is math.nan)


# Deprecated, use math namespace instead
abs = core.abs
exp = core.exp
log = core.log
log10 = core.log10


