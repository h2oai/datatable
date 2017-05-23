#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .base_expr import BaseExpr
from .consts import nas_map



class ColSelectorExpr(BaseExpr):
    """
    Column selector expression node.
    """

    def __init__(self, dtexpr, colindex):
        super().__init__()
        self._dtexpr = dtexpr
        self._colid = colindex
        self._stype = dtexpr.stypes[colindex]
        self._view_colid = dtexpr.get_column_srcindex(self._colid)

    @property
    def col_index(self):
        return self._colid


    def _value(self, key, inode):
        v = inode.make_keyvar(key, key, exact=True)
        inode.check_num_rows(self._dtexpr.nrows)
        assert v == key
        if self._view_colid is None:
            datavar = key + "_data"
            inode.make_keyvar(datavar, datavar, exact=True)
            dtcols = self._get_dt_columns(inode)
            inode.addto_preamble("{type} *{data} = {columns}[{idx}]->data;"
                                 .format(type=self.ctype, data=datavar,
                                         columns=dtcols, idx=self._colid))
            inode.addto_mainloop("{type} {var} = {data}[i];"
                                 .format(type=self.ctype, var=v, data=datavar))
        else:
            # needs fixing
            srccols, idxloop = inode.get_dt_srccolumns(self._dtexpr)
            data_array = "%s[%d]" % (srccols, self._view_colid)
            inode.addto_preamble("%s *%s_data = %s->data;" %
                                 (self.ctype, v, data_array))
            inode.addto_mainloop("%s %s = %s_data[%s];  // %s" %
                                 (self.ctype, v, v, idxloop, self))
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

    def __str__(self):
        # This will be used both as a key for IteratorNode memoizer, and the
        # name of the variable that contains the value of the column (for `i`-th
        # row). The name will look as follows:
        #     d3_salary    or    d3_17
        # where "d3" is the id of the datatable, and "salary" / "17" is the
        # column name/ index.
        #
        colname = self._dtexpr.names[self._colid]
        if len(colname) > 12 or not colname.isalnum():
            colname = str(self._colid)
        return str(self._dtexpr) + "_" + colname


    def _get_dt_columns(self, inode):
        dt = self._dtexpr.get_datatable()  # type: DataTable
        key = str(self._dtexpr) + "columns"
        var = inode.get_keyvar(key)
        if not var:
            var = inode.make_keyvar(key, key, exact=True)
            vdt = inode.get_dtvar(dt)
            inode.addto_preamble("Column **{var} = {dt}->columns;"
                                 .format(var=var, dt=vdt))
        return var
