#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
from .base_expr import BaseExpr
from .consts import nas_map
from ..types import stype
from datatable.lib import core


class LiteralExpr(BaseExpr):
    __slots__ = ["arg"]

    def __init__(self, arg):
        super().__init__()
        self.arg = arg
        # Determine smallest type
        if arg is True or arg is False or arg is None:
            self._stype = stype.bool8
        elif isinstance(arg, int):
            aarg = abs(arg)
            if aarg < 128:
                self._stype = stype.int8
            elif aarg < 32768:
                self._stype = stype.int16
            elif aarg < 1 << 31:
                self._stype = stype.int32
            elif aarg < 1 << 63:
                self._stype = stype.int64
            else:
                self._stype = stype.float64
        elif isinstance(arg, float):
            aarg = abs(arg)
            if aarg < 3.4e38:
                self._stype = stype.float32
            else:
                self._stype = stype.float64
        else:
            raise TypeError("Cannot use value %r in the expression" % arg)

    def resolve(self):
        pass

    def evaluate_eager(self):
        return core.column_from_list([self.arg])

    def _isna(self, key, block):
        return self.arg is None


    def _notna(self, key, block):
        return str(self.arg)


    def _value(self, key, block):
        if self.arg is None:
            return nas_map[self.stype]
        else:
            return str(self.arg)


    def __str__(self):
        return self._value(None, None)
