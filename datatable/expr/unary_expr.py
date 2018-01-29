#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .base_expr import BaseExpr
from .consts import unary_ops_rules
from ..types import stype
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
            raise TypeError("Operation %s not allowed on operands of type %s"
                            % (self._op, self._arg.stype))
        if self._op == "~" and self._stype == stype.bool8:
            self._op = "!"

    def evaluate_eager(self):
        arg = self._arg.evaluate_eager()
        opcode = unary_op_codes[self._op]
        return core.expr_unaryop(opcode, arg)


    def _isna(self, key, block):
        return self._arg.isna(block)


    def _notna(self, key, block):
        return "(%s %s)" % (self._op, self._arg.notna(block))


    def __str__(self):
        return "(%s %s)" % (self._op, self._arg)



unary_op_codes = {
    "isna": 1,
    "-": 2,
    "+": 3,
    "~": 4,
}
