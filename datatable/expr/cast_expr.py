#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
from .base_expr import BaseExpr
from datatable.lib import core

__all__ = ("CastExpr", )

# See "c/expr/base_expr.h"
BASEEXPR_OPCODE_CAST = 5



class CastExpr(BaseExpr):
    __slots__ = ["_arg"]

    def __init__(self, arg, stype):
        super().__init__()
        self._arg = arg
        self._stype = stype

    def __str__(self):
        return "%s(%s)" % (self._stype.name, str(self._arg))

    def _core(self):
        return core.base_expr(BASEEXPR_OPCODE_CAST,
                              self._arg._core(),
                              self._stype.value)
