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
from datatable.utils.typechecks import TTypeError


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

    def resolve(self):
        dt = self._dtexpr.get_datatable()
        self._colid = dt.colindex(self._colexpr)
        self._stype = self._dtexpr.stypes[self._colid]

    def is_reduce_expr(self, ee):
        return self._colid in ee.groupby_cols

    @property
    def col_index(self):
        return self._colid

    def __str__(self):
        strf = str(self._dtexpr)
        name = self._colexpr
        if isinstance(name, str) and ColSelectorExpr._colname_ok(name):
            return "%s.%s" % (strf, name)
        else:
            return "%s[%r]" % (strf, name)

    def safe_name(self):
        # This will be used as the name of the variable that contains the value
        # of the column (for `i`-th row). The name will look as follows:
        #     f_salary    or    f_17
        # where "f" is the id of the datatable, and "salary" / "17" is the
        # column name/ index.
        # The only reason we don't always use the latter form is to improve
        # code readability during debugging.
        #
        # self._colexpr can be either a column name, or a column id. We must be
        # careful to ensure that this method never throws an error, even if the
        # _dtexpr is unbound, or if the referenced column does not exist.
        #
        dt = self._dtexpr.get_datatable()
        if not dt:
            return str(self)
        colid = self._colexpr
        if isinstance(colid, int):
            if colid >= dt.ncols:
                colname = str(colid)
            elif colid < -dt.ncols:
                colname = str(-colid) + "_"
            else:
                colname = dt.names[colid]
                if not ColSelectorExpr._colname_ok(colname):
                    colname = str(colid % dt.ncols)
        else:
            colname = colid
            if not ColSelectorExpr._colname_ok(colname):
                try:
                    colname = str(dt.colindex(colname))
                except ValueError:
                    colname = "_" + str(id(colname))
        return str(self._dtexpr) + "_" + colname

    @staticmethod
    def _colname_ok(name):
        return (0 < len(name) < 13 and
                name.isalnum() and  # '_' are not considered alphanumeric
                not name.isdigit())



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
        v = inode.make_keyvar(key, self.safe_name() + "_isna", exact=True)
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
        ri = self._dtexpr.get_rowindex()
        print("Evaluate eager: %r -> dt=%r, ri=%r" % (self, dt, ri))
        return core.expr_column(dt.internal, self._colid, ri)




class NewColumnExpr(BaseExpr):
    __slots__ = ["_name"]

    def __init__(self, name):
        self._name = name

    def __str__(self):
        return self._name

    def resolve(self):
        pass
