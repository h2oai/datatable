#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
from .base_expr import BaseExpr
from datatable.lib import core
from datatable.utils.typechecks import TTypeError

# See "c/expr/base_expr.h"
BASEEXPR_OPCODE_COLUMN = 1



class ColSelectorExpr(BaseExpr):
    """
    Expression node that selects a particular Column from a Frame.

    This node is usually created from the following expressions:
        f.colName
        f[3]
        f["any name"]
    """
    __slots__ = ["_dtexpr", "_colexpr", "_colid"]

    def __init__(self, dtexpr, selector):
        super().__init__()
        self._dtexpr = dtexpr
        self._colexpr = selector
        self._colid = None
        if not isinstance(selector, (int, str)):
            raise TTypeError("Column selector should be an integer or "
                             "a string, not %r" % type(selector))

    def _core(self):
        return core.base_expr(BASEEXPR_OPCODE_COLUMN,
                              self._dtexpr._id, self._colexpr)

    def __str__(self):
        strf = str(self._dtexpr)
        name = self._colexpr
        if isinstance(name, str) and ColSelectorExpr._colname_ok(name):
            return "%s.%s" % (strf, name)
        else:
            return "%s[%r]" % (strf, name)


    @staticmethod
    def _colname_ok(name):
        return (0 < len(name) < 13 and
                name.isalnum() and  # '_' are not considered alphanumeric
                not name.isdigit())




class NewColumnExpr(BaseExpr):
    __slots__ = ["_name"]

    def __init__(self, name):
        self._name = name

    def __str__(self):
        return self._name
