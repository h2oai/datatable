#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import math
from .base_expr import BaseExpr
from .unary_expr import unary_op_codes
from ..utils.typechecks import TTypeError, Frame_t, is_type
from ..types import stype
from datatable.lib import core

__all__ = ("isna", )


class Isna(BaseExpr):
    __slots__ = ["_arg"]

    def __init__(self, arg):
        super().__init__()
        self._arg = arg

    def resolve(self):
        self._arg.resolve()
        self._stype = stype.bool8

    def is_reduce_expr(self, ee):
        return self._arg.is_reduce_expr(ee)

    def evaluate_eager(self, ee):
        arg = self._arg.evaluate_eager(ee)
        opcode = unary_op_codes["isna"]
        return core.expr_unaryop(opcode, arg)

    def _isna(self, key, inode):
        # The function always returns either True or False but never NA
        return False


    def _notna(self, key, inode):
        return self._arg.isna(inode)


    def _value(self, key, inode):
        return self._arg.isna(inode)


    def __str__(self):
        return "isna(%s)" % self._arg



def isna(x):
    if isinstance(x, BaseExpr):
        return Isna(x)
    if is_type(x, Frame_t):
        if x.ncols != 1:
            raise TTypeError("Frame must have a single column")
        return x(select=lambda f: isna(f[0]))
    if x is None or (isinstance(x, float) and math.isnan(x)):
        return True
    return False
