#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------

from .base_expr import BaseExpr
from .consts import unary_ops_rules, unary_op_codes, baseexpr_opcodes
from ..types import stype
from datatable.utils.typechecks import TTypeError
from datatable.lib import core


class UnaryOpExpr(BaseExpr):
    __slots__ = ["_op", "_arg"]

    def __init__(self, op, arg):
        super().__init__()
        self._op = op
        self._arg = arg


    def __str__(self):
        return "(%s %s)" % (self._op, self._arg)


    def _core(self):
        return core.base_expr(baseexpr_opcodes["unop"],
                              unary_op_codes[self._op],
                              self._arg._core())
