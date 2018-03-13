#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------

from .base_expr import BaseExpr
from .consts import unary_ops_rules, unary_op_codes
from ..types import stype
from datatable.utils.typechecks import TTypeError
from datatable.lib import core


class UnaryOpExpr(BaseExpr):
    __slots__ = ["_op", "_arg"]

    def __init__(self, op, arg):
        super().__init__()
        self._op = op
        self._arg = arg

    def resolve(self):
        self._arg.resolve()
        self._stype = unary_ops_rules.get((self._op, self._arg.stype), None)
        if self._stype is None:
            raise TTypeError("Operator `%s` cannot be applied to a `%s` column"
                             % (self._op, self._arg.stype.name))
        if self._op == "~" and self._stype == stype.bool8:
            self._op = "!"

    def evaluate_eager(self, ee):
        arg = self._arg.evaluate_eager(ee)
        opcode = unary_op_codes[self._op]
        return core.expr_unaryop(opcode, arg)


    def _isna(self, key, block):
        return self._arg.isna(block)


    def _notna(self, key, block):
        return "(%s %s)" % (self._op, self._arg.notna(block))


    def __str__(self):
        return "(%s %s)" % (self._op, self._arg)
