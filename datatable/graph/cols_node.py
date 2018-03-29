#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import types

from datatable.lib import core
from .context import LlvmEvaluationEngine
from .iterator_node import MapNode
from datatable.expr import BaseExpr, ColSelectorExpr
from datatable.graph.dtproxy import f
from datatable.types import ltype, stype
from datatable.utils.misc import plural_form as plural
from datatable.utils.misc import normalize_slice
from datatable.utils.typechecks import TValueError, TTypeError



#===============================================================================

class ColumnSetNode:
    """
    Base class for nodes that create columns of a datatable.

    A ColumnSetNode encapsulates the `select` or `update` arguments of the main
    datatable evaluation function. In the C layer it creates a function that
    constructs and returns a ``Column**`` array of columns.
    """

    def __init__(self, ee):
        self._engine = ee
        self._column_names = tuple()

    @property
    def dt(self):
        return self._engine.dt

    @property
    def column_names(self):
        return self._column_names

    def execute(self):
        self._engine.columns = self._compute_columns()

    def _compute_columns(self):
        raise NotImplementedError




#===============================================================================

class SliceCSNode(ColumnSetNode):

    def __init__(self, ee, start, count, step):
        super().__init__(ee)
        self._start = start
        self._step = step
        self._count = count
        self._column_names = self._make_column_names()


    def _make_column_names(self):
        if self._step == 0:
            if self._count == 0:
                return tuple()
            s = self.dt.names[self._start]
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
            return self.dt.names[self._start:end:self._step]


    def _compute_columns(self):
        res = core.columns_from_slice(self.dt.internal, self._engine.rowindex,
                                      self._start, self._count, self._step)
        return res


    def get_list(self):
        start, count, step = self._start, self._count, self._step
        if step > 0:
            return list(range(start, start + count * step, step))
        elif step < 0:
            return list(range(start + (count - 1) * step, start - step, -step))
        else:
            return [start] * count


    def is_all(self):
        return (self._start == 0 and
                self._step == 1 and
                self._count == self.dt.ncols)



#===============================================================================

class ArrayCSNode(ColumnSetNode):

    def __init__(self, ee, elems, colnames):
        super().__init__(ee)
        self._elems = elems
        self._column_names = colnames

    def _compute_columns(self):
        return core.columns_from_array(self.dt.internal, self._engine.rowindex,
                                       self._elems)

    def get_list(self):
        return self._elems



#===============================================================================

class MixedCSNode(ColumnSetNode):

    def __init__(self, ee, elems, names):
        super().__init__(ee)
        self._elems = elems
        self._column_names = names
        self._rowindex = None
        expr_elems = []
        for elem in elems:
            if isinstance(elem, BaseExpr):
                elem.resolve()
                expr_elems.append(elem)
        self._mapnode = MapNode(ee.dt, expr_elems)
        self._mapnode.use_cmodule(ee)

    def _compute_columns(self):
        if isinstance(self._engine, LlvmEvaluationEngine):
            fnptr = self._mapnode.get_result()
            rowindex = self._engine.rowindex
            if rowindex:
                nrows = rowindex.nrows
            else:
                nrows = self.dt.nrows
            return core.columns_from_mixed(self._elems, self.dt.internal,
                                           nrows, fnptr)
        else:
            ee = self._engine
            _dt = ee.dt.internal
            _ri = ee.rowindex
            columns = [core.expr_column(_dt, e, _ri) if isinstance(e, int) else
                       e.evaluate_eager(ee)
                       for e in self._elems]
            return core.columns_from_columns(columns)




#===============================================================================

def make_columnset(arg, ee, _nested=False):
    """
    Create a :class:`CSNode` object from the provided expression.

    This is a factory function that instantiates an appropriate subclass of
    :class:`CSNode`, depending on the parameter ``arg`` and provided that it
    is applied to a Frame ``dt``.

    Parameters
    ----------
    arg: Any
        An expression that will be converted into one of the ``CSNode``s.

    ee: EvalutionEngine
        Expression evaluation engine.

    _nested: bool
        Internal flag which is set to True on the first recursive call.
    """
    dt = ee.dt

    if arg is None or arg is Ellipsis:
        return SliceCSNode(ee, 0, dt.ncols, 1)

    if arg is True or arg is False:
        # Note: True/False are integer objects in Python, hence this test has
        # to be performed before `isinstance(arg, int)` below.
        raise TTypeError("A boolean cannot be used as a column selector")

    if isinstance(arg, (int, str, slice, BaseExpr)):
        # Type of the processed column is `U(int, (int, int, int), BaseExpr)`
        pcol = process_column(arg, dt)
        if isinstance(pcol, int):
            return SliceCSNode(ee, pcol, 1, 1)
        elif isinstance(pcol, tuple):
            return SliceCSNode(ee, *pcol)
        else:
            assert isinstance(pcol, BaseExpr), "pcol: %r" % (pcol,)
            return MixedCSNode(ee, [pcol], names=["V0"])

    if isinstance(arg, (types.GeneratorType, list, tuple)):
        isarray = True
        outcols = []
        colnames = []
        for col in arg:
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
                assert isinstance(pcol, BaseExpr)
                pcol.resolve()
                isarray = False
                outcols.append(pcol)
                colnames.append(str(col))
        if isarray:
            return ArrayCSNode(ee, outcols, colnames)
        else:
            return MixedCSNode(ee, outcols, colnames)

    if isinstance(arg, dict):
        isarray = True
        outcols = []
        colnames = []
        for name, col in arg.items():
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
            return ArrayCSNode(ee, outcols, colnames)
        else:
            return MixedCSNode(ee, outcols, colnames)

    if isinstance(arg, types.FunctionType) and not _nested:
        res = arg(f)
        return make_columnset(res, ee, _nested=True)

    if isinstance(arg, (type, ltype)):
        ltypes = dt.ltypes
        lt = ltype(arg)
        outcols = []
        colnames = []
        for i in range(dt.ncols):
            if ltypes[i] == lt:
                outcols.append(i)
                colnames.append(dt.names[i])
        return ArrayCSNode(ee, outcols, colnames)

    if isinstance(arg, stype):
        stypes = dt.stypes
        outcols = []
        colnames = []
        for i in range(dt.ncols):
            if stypes[i] == arg:
                outcols.append(i)
                colnames.append(dt.names[i])
        return ArrayCSNode(ee, outcols, colnames)

    raise TValueError("Unknown `select` argument: %r" % arg)



def process_column(col, df):
    """
    Helper function to verify the validity of a single column selector.

    Given frame `df` and a column description `col`, this function returns:
      * either the numeric index of the column
      * a numeric slice, as a triple (start, count, step)
      * or a `BaseExpr` object
    """
    if isinstance(col, int):
        ncols = df.ncols
        if -ncols <= col < ncols:
            return col % ncols
        else:
            raise TValueError(
                "Column index `{col}` is invalid for a frame with {ncolumns}"
                .format(col=col, ncolumns=plural(ncols, "column")))

    if isinstance(col, str):
        # This raises an exception if `col` cannot be found in the dataframe
        return df.colindex(col)

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
                col0 = df.colindex(start)
            if stop is None:
                col1 = df.ncols - 1
            elif isinstance(stop, str):
                col1 = df.colindex(stop)
            if col0 is None or col1 is None:
                raise TValueError("Slice %r is invalid: cannot mix numeric and "
                                  "string column names" % col)
            if step is not None:
                raise TValueError("Column name slices cannot use strides: %r"
                                  % col)
            return (col0, abs(col1 - col0) + 1, 1 if col1 >= col0 else -1)
        elif all(x is None or isinstance(x, int) for x in (start, stop, step)):
            return normalize_slice(col, df.ncols)
        else:
            raise TValueError("%r is not integer-valued" % col)

    if isinstance(col, ColSelectorExpr):
        col.resolve()
        return col.col_index

    if isinstance(col, BaseExpr):
        return col

    raise TTypeError("Unknown column selector: %r" % col)
