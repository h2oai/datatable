#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import types

from .node import Node
from datatable.expr import DatatableExpr, BaseExpr
from datatable.utils.misc import plural_form as plural
from datatable.utils.misc import normalize_slice



#===============================================================================

class ColumnSetNode(Node):
    """
    Base class for nodes that create columns of a datatable.

    A ColumnSetNode encapsulates the `select` or `update` arguments of the main
    datatable evaluation function. In the C layer it creates a function that
    constructs and returns a ``Column**`` array of columns.
    """

    def __init__(self, dt):
        super().__init__()
        self._dt = dt
        self._cname = None
        self._n_columns = 0
        self._n_view_columns = 0

    @property
    def dt(self):  # is this used anywhere?
        return self._dt

    @property
    def n_columns(self):
        return self._n_columns

    @property
    def n_view_columns(self):
        return self._n_view_columns



#===============================================================================

class SliceView_CSNode(ColumnSetNode):

    def __init__(self, dt, start, count, step):
        super().__init__(dt)
        self._start = start
        self._step = step
        self._n_columns = count
        self._n_view_columns = count  # All columns are view columns

    @property
    def column_names(self):
        if self._step == 0:
            s = self._dt.names[self._start]
            return tuple([s] * self._n_columns)
        else:
            end = self._start + self._n_columns * self._step
            return self._dt.names[self._start:end:self._step]

    def cget_columns(self):
        """
        Return a name of C function that will create a `Column**` array.

        More specifically, this function will insert into the current evaluation
        context the C code of a function with the following signature:
            Column** get_columns(void);
        This function will allocate and fill the `Column**` array, and then
        relinquish the ownership of that pointer to the caller.
        """
        if not self._cname:
            self._cname = self._gen_c()
        return self._cname


    def _gen_c(self):
        varname = self.context.make_variable_name()
        fnname = "get_" + varname
        ncols = self._n_columns
        dt_isview = self._dt.internal.isview
        if not self.context.has_function(fnname):
            dtvar = self.context.get_dtvar(self._dt)
            fn = "static Column** %s(void) {\n" % fnname
            fn += "    ViewColumn **cols = calloc(%d, sizeof(ViewColumn*));\n" \
                  % (ncols + 1)
            if dt_isview:
                fn += "    Column **srccols = {dt}->source->columns;\n" \
                      .format(dt=dtvar)
            else:
                fn += "    Column **srccols = {dt}->columns;\n".format(dt=dtvar)
            fn += "    if (cols == NULL) return NULL;\n"
            fn += "    int64_t j = %dL;\n" % self._start
            fn += "    for (int64_t i = 0; i < %d; i++) {\n" % ncols
            fn += "        cols[i] = malloc(sizeof(ViewColumn));\n"
            fn += "        if (cols[i] == NULL) return NULL;\n"
            fn += "        cols[i]->srcindex = j;\n"
            fn += "        cols[i]->mtype = MT_VIEW;\n"
            fn += "        cols[i]->stype = srccols[j]->stype;\n"
            fn += "        j += %dL;\n" % self._step
            fn += "    }\n"
            fn += "    cols[%d] = NULL;\n" % ncols
            fn += "    return (Column**) cols;\n"
            fn += "}\n"
            self.context.add_function(fnname, fn)
        return fnname



#===============================================================================

class Mixed_CSNode(ColumnSetNode):

    def __init__(self, dt, elems):
        super().__init__(dt)
        self._elems = elems
        self._n_columns = len(elems)
        self._n_view_columns = sum(isinstance(x, int) for x in elems)





#===============================================================================

def make_columnset(cols, dt, _nested=False):
    if cols is Ellipsis:
        return SliceView_CSNode(dt, 0, dt.ncols, 1)

    if isinstance(cols, (int, str, slice, BaseExpr)):
        # Type of the processed column is `U(int, (int, int, int), BaseExpr)`
        pcol = process_column(cols, dt)
        if isinstance(pcol, int):
            return SliceView_CSNode(dt, pcol, 1, 1)
        elif isinstance(pcol, tuple):
            return SliceView_CSNode(dt, *pcol)
        else:
            assert isinstance(pcol, BaseExpr)
            return Mixed_CSNode(dt, [pcol])

    if isinstance(cols, (list, tuple)):
        out = []
        for col in cols:
            pcol = process_column(col, dt)
            if isinstance(pcol, int):
                out.append((pcol, dt.names[pcol]))
            elif isinstance(pcol, tuple):
                start, count, step = pcol
                for i in range(count):
                    j = start + i * step
                    out.append((j, dt.names[j]))
            else:
                out.append((pcol, str(col)))
        return out

    if isinstance(cols, types.FunctionType) and not _nested:
        res = cols(DatatableExpr(dt))
        return make_columnset(res, dt, nested=True)

    raise ValueError("Unknown `select` argument: %r" % cols)



def process_column(col, dt):
    """
    Helper function to verify the validity of a single column selector.

    Given datatable `dt` and a column description `col`, this function returns:
      * either the numeric index of the column
      * a numeric slice, as a triple (start, count, step)
      * or a `BaseExpr` object
    """
    if isinstance(col, int):
        ncols = dt.ncols
        if -ncols <= col < ncols:
            return col % ncols
        else:
            raise ValueError("Column index {col} is invalid for a datatable "
                             "with {ncolumns}"
                             .format(col=col, ncolumns=plural(ncols, "column")))

    if isinstance(col, str):
        # This raises an exception if `col` cannot be found in the datatable
        return dt.colindex(col)

    if isinstance(col, slice):
        start = col.start
        stop = col.stop
        step = col.step
        if isinstance(start, str) or isinstance(stop, str):
            col0 = None
            col1 = None
            if start is None:
                col0 = 0
            elif isinstance(start, str):
                col0 = dt.colindex(start)
            if stop is None:
                col1 = dt.ncols - 1
            elif isinstance(stop, str):
                col1 = dt.colindex(stop)
            if col0 is None or col1 is None:
                raise ValueError("Slice %r is invalid: cannot mix numeric and "
                                 "string column names" % col)
            if step is not None:
                raise ValueError("Column name slices cannot use "
                                 "strides: %r" % col)
            return (col0, abs(col1 - col0) + 1, 1 if col1 >= col0 else -1)
        elif all(x is None or isinstance(x, int) for x in (start, stop, step)):
            return normalize_slice(col, dt.ncols)
        else:
            raise ValueError("%r is not integer-valued" % col)

    if isinstance(col, BaseExpr):
        return col

    raise TypeError("Unknown column format: %r" % col)
