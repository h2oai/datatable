#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import types

import _datatable
from .node import Node
from .iterator_node import MapNode
from datatable.expr import DatatableExpr, BaseExpr, ColSelectorExpr
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
        self._rowmapping = None
        self._rowmappingdt = None
        self._cname = None
        self._n_columns = 0
        self._n_view_columns = 0
        self._column_names = tuple()

    @property
    def dt(self):
        # TODO: merge self._dt with self._rowmappingdt
        return self._dt

    @property
    def n_columns(self):
        return self._n_columns

    @property
    def n_view_columns(self):
        return self._n_view_columns

    @property
    def column_names(self):
        assert len(self._column_names) == self._n_columns
        return self._column_names

    def use_rowmapping(self, rowmapping, dt):
        self._rowmapping = rowmapping
        self._rowmappingdt = dt



#===============================================================================

class SliceView_CSNode(ColumnSetNode):

    def __init__(self, dt, start, count, step):
        super().__init__(dt)
        self._start = start
        self._step = step
        self._n_columns = count
        self._n_view_columns = count  # All columns are view columns
        self._column_names = self._make_column_names()


    def _make_column_names(self):
        if self._step == 0:
            s = self._dt.names[self._start]
            return tuple([s] * self._n_columns)
        else:
            end = self._start + self._n_columns * self._step
            return self._dt.names[self._start:end:self._step]


    def get_result(self):
        res = _datatable.columns_from_slice(self._dt.internal, self._start,
                                            self._n_columns, self._step)
        return res



#===============================================================================

class Mixed_CSNode(ColumnSetNode):

    def __init__(self, dt, elems, names):
        super().__init__(dt)
        self._elems = elems
        self._n_columns = len(elems)
        self._n_view_columns = sum(isinstance(x, int) for x in elems)
        self._column_names = names

    def _gen_c(self):
        fnname = "get_columns"
        if not self.soup.has_function(fnname):
            mapnode = MapNode([elem for elem in self._elems
                               if isinstance(elem, BaseExpr)],
                              rowmapping=self._rowmapping)
            mapnode.use_context(self.soup)
            mapfn = mapnode.generate_c()
            dtvar = self.soup.get_dtvar(self._dt)
            rowmapping = self._rowmapping.cget_rowmapping()
            fn = ("static Column** {fnname}(void) {{\n"
                  "    PyObject *elems = (PyObject*) {elemsptr}L;\n"
                  "    RowMapping *rowmapping = {rowmapping_getter}();\n"
                  "    return columns_from_pymixed(elems, {dt}, rowmapping, "
                  "&{mapfn});\n"
                  "}}\n"
                  .format(fnname=fnname, mapfn=mapfn, elemsptr=id(self._elems),
                          dt=dtvar, rowmapping_getter=rowmapping))
            self.soup.add_function(fnname, fn)
            self.soup.add_extern("columns_from_pymixed")
        return fnname




#===============================================================================

def make_columnset(cols, dt, _nested=False):
    if cols is Ellipsis or cols is None:
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
            return Mixed_CSNode(dt, [pcol], names=[str(cols)])

    if isinstance(cols, (list, tuple)):
        outcols = []
        colnames = []
        for col in cols:
            pcol = process_column(col, dt)
            if isinstance(pcol, int):
                outcols.append(pcol)
                colnames.append(dt.names[pcol])
            elif isinstance(pcol, tuple):
                start, count, step = pcol
                for i in range(count):
                    j = start + i * step
                    outcols.append(j)
                    colnames.append(dt.names[j])
            else:
                outcols.append(pcol)
                colnames.append(str(col))
        return Mixed_CSNode(dt, outcols, colnames)

    if isinstance(cols, types.FunctionType) and not _nested:
        res = cols(DatatableExpr(dt))
        return make_columnset(res, dt, _nested=True)

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

    if isinstance(col, ColSelectorExpr):
        return col.col_index

    if isinstance(col, BaseExpr):
        return col

    raise TypeError("Unknown column format: %r" % col)
