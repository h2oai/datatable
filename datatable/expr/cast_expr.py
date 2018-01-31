#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------

from .base_expr import BaseExpr
from datatable.lib import core

__all__ = ("CastExpr", )



class CastExpr(BaseExpr):
    __slots__ = ["_arg"]

    def __init__(self, arg, stype):
        super().__init__()
        self._arg = arg
        self._stype = stype

    def resolve(self):
        self._arg.resolve()

    def evaluate_eager(self):
        col = self._arg.evaluate_eager()
        return core.expr_cast(col, self._stype.value)
