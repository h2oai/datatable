#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .base_expr import BaseExpr
from ..utils.typechecks import TTypeError, DataTable_t, is_type
from ..types import stype

__all__ = ("isna", )


class Isna(BaseExpr):

    def __init__(self, arg):
        super().__init__()
        self._arg = arg
        self._stype = stype.bool8


    def _isna(self, key, block):
        # The function always returns either True or False but never NA
        return False


    def _notna(self, key, block):
        return self._arg.isna(block)


    def _value(self, key, block):
        return self._arg.isna(block)


    def __str__(self):
        return "isna(%s)" % self._arg


def isna(x):
    if x is None:
        return True
    if isinstance(x, BaseExpr):
        return Isna(x)
    if is_type(x, DataTable_t):
        if x.ncols != 1:
            raise TTypeError("DataTable must have a single column")
        return x(select=lambda f: isna(f[0]))
    return False
