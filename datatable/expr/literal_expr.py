#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
from .base_expr import BaseExpr
from .consts import nas_map, baseexpr_opcodes
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
        elif isinstance(arg, str):
            self._stype = stype.str32
        else:
            raise TypeError("Cannot use value %r in the expression" % arg)

    def resolve(self):
        pass

    def _core(self):
        return core.base_expr(baseexpr_opcodes["literal"], self.arg)

    def __str__(self):
        return self._value(None, None)
