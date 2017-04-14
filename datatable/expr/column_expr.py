#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from ._expr import ExprNode
from .consts import decimal_stypes, nas_map



class ColSelectorExpr(ExprNode):
    """
    Column selector expression node.
    """

    def __init__(self, dtexpr, colindex):
        super().__init__()
        self.dtexpr = dtexpr
        self.colid = colindex
        self.stype = dtexpr.stypes[colindex]
        self.scale = 0
        if self.stype in decimal_stypes:
            self.scale = 2  # TODO: read this from the column's meta
        self.view_colid = dtexpr.get_column_srcindex(self.colid)

    def _isna(self, block):
        # TODO: use rollup stats to determine if some variable is never NA
        val = self.value(block)
        isna = val + "_isna"
        na = nas_map[self.stype]
        if na == "NA_F32" or na == "NA_F64":
            block.add_mainloop_expr("int %s = IS%s(%s);" % (isna, na, val))
        else:
            block.add_mainloop_expr("int %s = (%s == %s);" % (isna, val, na))
        return isna

    def _notna(self, block):
        return self.value(block)

    def _value(self, block):
        v = block.make_variable("v")
        if self.view_colid is None:
            columns = block.get_dt_columns(self.dtexpr)
            data_array = "%s[%d]" % (columns, self.colid)
            idxloop = "i"
        else:
            srccols, idxloop = block.get_dt_srccolumns(self.dtexpr)
            data_array = "%s[%d]" % (srccols, self.view_colid)
        block.add_prologue_expr("%s *%s_data = %s.data;" %
                                (self.ctype, v, data_array))
        block.add_mainloop_expr("%s %s = %s_data[%s];  // %s" %
                                (self.ctype, v, v, idxloop, self))
        return v

    def __str__(self):
        colname = self.dtexpr.names[self.colid]
        return (("%s.%s" if colname.isalnum() else "%s.%r") %
                (self.dtexpr.get_dtname(), colname))
