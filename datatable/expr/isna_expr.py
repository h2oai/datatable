#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from ._expr import ExprNode

__all__ = ("isna", )


class Isna(ExprNode):

    def __init__(self, arg):
        super().__init__()
        self._arg = arg
        self.stype = "i1b"


    def _isna(self, block):
        # The function always returns either True or False but never NA
        return False


    def _notna(self, block):
        return self._arg.isna(block)


    def _value(self, block):
        return self._arg.isna(block)


    def __str__(self):
        return "isna(%s)" % self._arg


def isna(x):
    if x is None:
        return True
    if isinstance(x, ExprNode):
        return Isna(x)
    return False
