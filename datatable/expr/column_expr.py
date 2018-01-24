#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .base_expr import BaseExpr
from .consts import nas_map
import datatable.lib._datatable as core



class ColSelectorExpr(BaseExpr):
    """
    Expression node that selects a particular Column from a DataTable.

    This node is usually created from the following expressions:
        f.colName
        f[3]
        f["any name"]
    """
    __slots__ = ("_dtexpr", "_colid")

    def __init__(self, dtexpr, colindex):
        super().__init__()
        self._dtexpr = dtexpr    # type: DatatableExpr
        self._colid = colindex   # type: int
        self._stype = dtexpr.stypes[colindex]

    @property
    def col_index(self):
        return self._colid

    def __str__(self):
        # This will be used both as a key for node memoization, and the name of
        # the variable that contains the value of the column (for `i`-th row).
        # The name will look as follows:
        #     d3_salary    or    d3_17
        # where "d3" is the id of the datatable, and "salary" / "17" is the
        # column name/ index.
        # The only reason we don't always use the latter form is to improve
        # code readability during debugging.
        #
        colname = self._dtexpr.names[self._colid]
        if len(colname) > 12 or not colname.isalnum() or colname.isdigit():
            colname = str(self._colid)
        return str(self._dtexpr) + "_" + colname


    #---------------------------------------------------------------------------
    # LLVM evaluation
    #---------------------------------------------------------------------------

    def _value(self, key, inode):
        v = inode.make_keyvar(key, key, exact=True)
        inode.check_num_rows(self._dtexpr.nrows)
        assert v == key
        datavar = key + "_data"
        inode.make_keyvar(datavar, datavar, exact=True)
        inode.addto_preamble("{type} *{data} = "
                             "({type}*) dt_column_data({dt}, {idx});"
                             .format(type=self.ctype, data=datavar,
                                     dt=self._get_dtvar(inode),
                                     idx=self._colid))
        inode.addto_mainloop("{type} {var} = {data}[i];"
                             .format(type=self.ctype, var=v, data=datavar))
        return v

    def _isna(self, key, inode):
        # TODO: use rollup stats to determine if some variable is never NA
        v = inode.make_keyvar(key, str(self) + "_isna", exact=True)
        isna_fn = "IS" + nas_map[self.stype]
        inode.add_extern(isna_fn)
        inode.addto_mainloop("int {var} = {isna}({value});"
                             .format(var=v, isna=isna_fn,
                                     value=self.value(inode)))
        return v

    def notna(self, inode):
        return self.value(inode)

    def _get_dtvar(self, inode):
        dt = self._dtexpr.get_datatable()
        return inode.get_dtvar(dt)


    #---------------------------------------------------------------------------
    # Eager evaluation
    #---------------------------------------------------------------------------

    def evaluate(self):
        dt = self._dtexpr.get_datatable()
        return core.expr_column(dt.internal, self._colid)
