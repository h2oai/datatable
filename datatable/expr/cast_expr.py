#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

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
