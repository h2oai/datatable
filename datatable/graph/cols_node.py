#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import types

import _datatable
from .node import Node
from .iterator_node import MapNode
from datatable.expr import DatatableExpr, BaseExpr, ColSelectorExpr
from datatable.utils.misc import plural_form as plural
from datatable.utils.misc import normalize_slice
from datatable.utils.typechecks import TValueError, TTypeError



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
        self._rowindex = None
        self._rowindexdt = None
        self._cname = None
        self._n_columns = 0
        self._n_view_columns = 0
        self._column_names = tuple()

    @property
    def dt(self):
        # TODO: merge self._dt with self._rowindexdt
        return self._dt

    @property
    def column_names(self):
        return self._column_names



#===============================================================================

class SliceCSNode(ColumnSetNode):

    def __init__(self, dt, start, count, step):
        super().__init__(dt)
        self._start = start
        self._step = step
        self._count = count
        self._column_names = self._make_column_names()


    def _make_column_names(self):
        if self._step == 0:
            s = self._dt.names[self._start]
            return tuple([s] * self._count)
        else:
            end = self._start + self._count * self._step
            if end < 0:
                # If step is negative, then `end` can become negative. However
                # slice() would reinterpret that negative index as counting from
                # the end, which is not what we want. For example triple
                # (3, 2, -2) denotes indices [3, 1], which can be achieved using
                # slice [3::-2] but not slice [3:-1:-2].
                end = None
            return self._dt.names[self._start:end:self._step]


    def get_result(self):
        res = _datatable.columns_from_slice(self._dt.internal, self._start,
                                            self._count, self._step)
        return res



#===============================================================================

class ArrayCSNode(ColumnSetNode):

    def __init__(self, dt, elems, colnames):
        super().__init__(dt)
        self._elems = elems
        self._column_names = colnames

    def get_result(self):
        return _datatable.columns_from_array(self._dt.internal, self._elems)



#===============================================================================

class MixedCSNode(ColumnSetNode):

    def __init__(self, dt, elems, names):
        super().__init__(dt)
        self._elems = elems
        self._column_names = names
        self._mapnode = None

    def _added_into_soup(self):
        self._rowindex = self.soup.get("rows")
        self._mapnode = MapNode([elem for elem in self._elems
                                 if isinstance(elem, BaseExpr)],
                                rowindex=self._rowindex)
        self.soup.add("columns_mapfn", self._mapnode)

    def get_result(self):
        fnptr = self._mapnode.get_result()
        rowindex = self._rowindex.get_result()
        if rowindex:
            nrows = rowindex.length
        else:
            nrows = self._dt.nrows
        return _datatable.columns_from_mixed(self._elems, self._dt.internal,
                                             nrows, fnptr)



#===============================================================================

def make_columnset(cols, dt, _nested=False):
    if cols is Ellipsis or cols is None:
        return SliceCSNode(dt, 0, dt.ncols, 1)

    if isinstance(cols, (int, str, slice, BaseExpr)):
        # Type of the processed column is `U(int, (int, int, int), BaseExpr)`
        pcol = process_column(cols, dt)
        if isinstance(pcol, int):
            return SliceCSNode(dt, pcol, 1, 1)
        elif isinstance(pcol, tuple):
            return SliceCSNode(dt, *pcol)
        else:
            assert isinstance(pcol, BaseExpr)
            return MixedCSNode(dt, [pcol], names=["V0"])

    if isinstance(cols, (list, tuple)):
        isarray = True
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
                isarray = False
                outcols.append(pcol)
                colnames.append(str(col))
        if isarray:
            return ArrayCSNode(dt, outcols, colnames)
        else:
            return MixedCSNode(dt, outcols, colnames)

    if isinstance(cols, dict):
        isarray = True
        outcols = []
        colnames = []
        for name, col in cols.items():
            pcol = process_column(col, dt)
            colnames.append(name)
            if isinstance(pcol, int):
                outcols.append(pcol)
            elif isinstance(pcol, tuple):
                start, count, step = pcol
                for i in range(count):
                    j = start + i * step
                    outcols.append(j)
                    if i > 0:
                        colnames.append(name + str(i))
            else:
                isarray = False
                outcols.append(pcol)
        if isarray:
            return ArrayCSNode(dt, outcols, colnames)
        else:
            return MixedCSNode(dt, outcols, colnames)

    if isinstance(cols, types.FunctionType) and not _nested:
        res = cols(DatatableExpr(dt))
        return make_columnset(res, dt, _nested=True)

    raise TValueError("Unknown `select` argument: %r" % cols)



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
            raise TValueError("Column index `{col}` is invalid for a datatable "
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
                raise TValueError("Slice %r is invalid: cannot mix numeric and "
                                  "string column names" % col)
            if step is not None:
                raise TValueError("Column name slices cannot use "
                                  "strides: %r" % col)
            return (col0, abs(col1 - col0) + 1, 1 if col1 >= col0 else -1)
        elif all(x is None or isinstance(x, int) for x in (start, stop, step)):
            return normalize_slice(col, dt.ncols)
        else:
            raise TValueError("%r is not integer-valued" % col)

    if isinstance(col, ColSelectorExpr):
        return col.col_index

    if isinstance(col, BaseExpr):
        return col

    raise TTypeError("Unknown column format: %r" % col)
