#!/usr/bin/env python
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#-------------------------------------------------------------------------------
from .base_expr import BaseExpr
# from datatable.frame import Frame
from datatable.lib import core

# See "c/expr/base_expr.h"
BASEEXPR_OPCODE_STRINGFN = 8



class StringExpr(BaseExpr):
    __slots__ = ["_op", "_expr", "_params"]

    def __init__(self, opcode, expr, *args):
        super().__init__()
        self._op = opcode
        self._expr = expr
        self._params = args

    def __str__(self):
        return "%s(%s, %r)" % (self._op, self._expr, self._params)

    def _core(self):
        return core.base_expr(BASEEXPR_OPCODE_STRINGFN,
                              string_opcodes[self._op],
                              self._expr._core(),
                              self._params)


string_opcodes = {
    "re_match": 1,
}
