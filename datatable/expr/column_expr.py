#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------

from .base_expr import BaseExpr
from .consts import nas_map
from ..types import stype
from datatable.lib import core



class ColSelectorExpr(BaseExpr):
    """
    Expression node that selects a particular Column from a Frame.

    This node is usually created from the following expressions:
        f.colName
        f[3]
        f["any name"]
    """
    __slots__ = ["_dtexpr", "_colid"]

    def __init__(self, dtexpr, selector):
        super().__init__()
        self._dtexpr = dtexpr
        self._colid = selector

    def resolve(self):
        if not self._stype:
            dt = self._dtexpr.get_datatable()
            self._colid = dt.colindex(self._colid)
            self._stype = self._dtexpr.stypes[self._colid]

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
        if self._dtexpr.get_datatable():
            colname = self._dtexpr.names[self._colid]
            if len(colname) > 12 or not colname.isalnum() or colname.isdigit():
                colname = str(self._colid)
        else:
            colname = str(self._colid)
        return str(self._dtexpr) + "_" + colname


    #---------------------------------------------------------------------------
    # LLVM evaluation
    #---------------------------------------------------------------------------

    def _value(self, key, inode):
        self.resolve()
        v = inode.make_keyvar(key, key, exact=True)
        inode.check_num_rows(self._dtexpr.nrows)
        assert v == key
        datavar = key + "_data"
        inode.make_keyvar(datavar, datavar, exact=True)
        inode.addto_preamble("{type}* {data} = "
                             "({type}*) dt_column_data({dt}, {idx});"
                             .format(type=self.ctype, data=datavar,
                                     dt=self._get_dtvar(inode),
                                     idx=self._colid))
        if self.stype == stype.str32:
            inode.addto_preamble("{data}++;".format(data=datavar))
        inode.addto_mainloop("{type} {var} = {data}[i];"
                             .format(type=self.ctype, var=v, data=datavar))
        return v

    def _isna(self, key, inode):
        self.resolve()
        # TODO: use rollup stats to determine if some variable is never NA
        v = inode.make_keyvar(key, str(self) + "_isna", exact=True)
        if self.stype == stype.str32:
            inode.addto_mainloop("int {var} = ({value} < 0);"
                                 .format(var=v, value=self.value(inode)))
        else:
            isna_fn = "IS" + nas_map[self.stype]
            inode.add_extern(isna_fn)
            inode.addto_mainloop("int {var} = {isna}({value});"
                                 .format(var=v, isna=isna_fn,
                                         value=self.value(inode)))
        return v

    def notna(self, inode):
        return self.value(inode)

    def _notna(self, key, inode):
        pass

    def _get_dtvar(self, inode):
        dt = self._dtexpr.get_datatable()
        return inode.get_dtvar(dt)


    #---------------------------------------------------------------------------
    # Eager evaluation
    #---------------------------------------------------------------------------

    def evaluate_eager(self, ee):
        self.resolve()
        dt = self._dtexpr.get_datatable()
        ri = ee.rowindex
        return core.expr_column(dt.internal, self._colid, ri)




class NewColumnExpr(BaseExpr):
    __slots__ = ["_name"]

    def __init__(self, name):
        self._name = name

    def __str__(self):
        return self._name

    def resolve(self):
        pass
