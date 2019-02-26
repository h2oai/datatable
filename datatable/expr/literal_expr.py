#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
from .base_expr import BaseExpr
from ..types import stype
from datatable.lib import core

# See "c/expr/base_expr.h"
BASEEXPR_OPCODE_LITERAL = 3



class LiteralExpr(BaseExpr):
    __slots__ = ["arg"]

    def __init__(self, arg):
        super().__init__()
        self.arg = arg

    def _core(self):
        return core.base_expr(BASEEXPR_OPCODE_LITERAL, self.arg)

    def __str__(self):
        return self._value(None, None)
