#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .base_expr import BaseExpr
from .consts import nas_map


class LiteralNode(BaseExpr):

    def __init__(self, arg):
        super().__init__()
        self.arg = arg
        # Determine smallest type
        if arg is True or arg is False or arg is None:
            self._stype = "i1b"
        elif isinstance(arg, int):
            aarg = abs(arg)
            if aarg < 128:
                self._stype = "i1i"
            elif aarg < 32768:
                self._stype = "i2i"
            elif aarg < 1 << 31:
                self._stype = "i4i"
            elif aarg < 1 << 63:
                self._stype = "i8i"
            else:
                self._stype = "f8r"
        elif isinstance(arg, float):
            aarg = abs(arg)
            sarg = str(aarg)
            ndec = len(sarg) - sarg.find(".") - 1
            if ndec <= 6:
                tenp = 10**ndec
                self.arg = int(arg * tenp + 0.001)
                self.scale = ndec
                assert self.arg / tenp == arg
                aarg = abs(self.arg)
                if aarg < 32768:
                    self._stype = "i2r"
                elif aarg < 1 << 31:
                    self._stype = "i4r"
                elif aarg < 1 << 63:
                    self._stype = "i8r"
                else:
                    self.arg = arg
                    self._stype = "f8r"
                    self.scale = 0
            elif aarg < 3.4e38:
                self._stype = "f4r"
            else:
                self._stype = "f8r"
        else:
            raise TypeError("Cannot use value %r in the expression" % arg)


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
