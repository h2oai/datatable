#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
# mean_expr, sd_expr, minmax_expr can eventually be merged into here too

from .base_expr import BaseExpr
from .consts import reduce_opcodes, ops_rules
from datatable.lib import core
from datatable.types import stype

_builtin_sum = sum

def sum(iterable, start=0):
    if isinstance(iterable, BaseExpr):
        return ReduceExpr("sum", iterable)
    else:
        return _builtin_sum(iterable, start)


def count(iterable=None):
    if isinstance(iterable, BaseExpr):
        return ReduceExpr("count", iterable)
    elif iterable is None:
        return CountExpr()
    else:
        return _builtin_sum((x is not None) for x in iterable)


def first(iterable):
    if isinstance(iterable, BaseExpr):
        return ReduceExpr("first", iterable)
    else:
        for x in iterable:
            return x


class CountExpr(BaseExpr):
    def is_reduce_expr(self, ee):
        return True

    def resolve(self):
        self._stype = stype.int64

    def evaluate_eager(self, ee):
        return core.expr_count(ee.dt.internal, ee.groupby)

    def __str__(self):
        return "count()"



class ReduceExpr(BaseExpr):
    __slots__ = ["_op", "_expr"]

    def __init__(self, op, expr):
        super().__init__()
        self._op = op
        self._expr = expr

    def is_reduce_expr(self, ee):
        return True

    def resolve(self):
        self._expr.resolve()
        expr_stype = self._expr.stype
        self._stype = ops_rules.get((self._op, expr_stype))
        if self._stype is None:
            raise ValueError(
                "Cannot compute %s of a variable of type %s"
                % (self._op, expr_stype))

    def evaluate_eager(self, ee):
        col = self._expr.evaluate_eager(ee)
        opcode = reduce_opcodes[self._op]
        return core.expr_reduceop(opcode, col, ee.groupby)


    def __str__(self):
        return "%s(%s)" % (self._op, self._expr)

