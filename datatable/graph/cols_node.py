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
from datatable.expr import BaseExpr, ColSelectorExpr, NewColumnExpr
from datatable.expr.consts import reduce_opcodes
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
        self._names = tuple()

    @property
    def dt(self):
        return self._engine.dt

    @property
    def column_names(self):
        return self._names

    def execute(self):
        self._engine.columns = self._compute_columns()

    def execute_update(self, dt, replacement):
        raise NotImplementedError

    def _compute_columns(self):
        raise NotImplementedError

    def is_all(self):
        return False



#===============================================================================

class AllCSNode(ColumnSetNode):

    def __init__(self, ee):
        super().__init__(ee)
        self._make_column_names()

    def __repr__(self):
        return "<datatable.graph.AllCSNode>"

    def _make_column_names(self):
        jdt = self._engine.joindt
        if jdt:
            self._names = self.dt.names + jdt.names[len(jdt.key):]
        else:
            self._names = self.dt.names

    def _compute_columns(self):
        res = core.columns_from_slice(self.dt.internal, self._engine.rowindex,
                                      0, self.dt.ncols, 1)
        jdt = self._engine.joindt
        if jdt:
            nk = len(jdt.key)
            res2 = core.columns_from_slice(jdt.internal, self._engine.joinindex,
                                           nk, jdt.ncols - nk, 1)
            res.append_columns(res2)
        return res


    def execute_update(self, dt, replacement):
        ri = self._engine.rowindex
        dt.internal.replace_column_slice(0, self.dt.ncols, 1,
                                         ri, replacement.internal)
        # Clear cached stypes/ltypes; No need to update names
        dt._stypes = None
        dt._ltypes = None


    def get_list(self):
        return list(range(self.dt.ncols))


    def is_all(self):
        return True





#===============================================================================

class SliceCSNode(ColumnSetNode):

    def __init__(self, ee, start, count, step):
        super().__init__(ee)
        self._start = start
        self._step = step
        self._count = count
        self._names = self._make_column_names()

    def __repr__(self):
        return ("<datatable.graph.SliceCSNode %d/%d/%d>"
                % (self._start, self._count, self._step))

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


    def execute_update(self, dt, replacement):
        ri = self._engine.rowindex
        dt.internal.replace_column_slice(self._start, self._count, self._step,
                                         ri, replacement.internal)
        # Clear cached stypes/ltypes; No need to update names
        dt._stypes = None
        dt._ltypes = None


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
        self._names = colnames

    def __repr__(self):
        return ("<datatable.graph.ArrayCSNode [%s]>"
                % ", ".join("%d (%r)" % (self._elems[i], self._names[i])
                            for i in range(len(self._elems))))

    def _compute_columns(self):
        ee = self._engine
        _dt = self.dt.internal
        cols = [_dt.column(i) for i in self._elems]
        if ee.get_source_rowindex():
            for _col in cols:
                ri = ee.get_final_rowindex(_col.rowindex)
                _col.replace_rowindex(ri)
        return core.columns_from_columns(cols)

    def get_list(self):
        return self._elems

    def execute_update(self, dt, replacement):
        n = dt.ncols
        ri = self._engine.rowindex
        dt.internal.replace_column_array(self._elems, ri, replacement.internal)
        new_names = list(dt.names)
        for i in range(len(self._elems)):
            j = self._elems[i]
            name = self._names[i]
            if j == -1:
                n += 1
                new_names.append(name)
            else:
                new_names[j] = name
        dt._fill_from_dt(dt.internal, names=new_names)
        assert dt.ncols == n



#===============================================================================

class MixedCSNode(ColumnSetNode):

    def __init__(self, ee, elems, names):
        super().__init__(ee)
        self._elems = elems
        self._names = names
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
            ncols = len(self._elems)
            if ee.groupby:
                opfirst = reduce_opcodes["first"]
                n_reduce_cols = 0
                for elem in self._elems:
                    if isinstance(elem, int):
                        is_groupby_col = elem in ee.groupby_cols
                        n_reduce_cols += is_groupby_col
                    else:
                        n_reduce_cols += elem.is_reduce_expr(ee)
                expand_dataset = (n_reduce_cols < ncols)
                columns = ee.groupby_cols + self._elems
                self._names = ([ee.dt.names[i] for i in ee.groupby_cols] +
                               self._names)
                for i, elem in enumerate(columns):
                    if isinstance(elem, int):
                        col = core.expr_column(_dt, elem, _ri)
                        if not expand_dataset:
                            col = core.expr_reduceop(opfirst, col, ee.groupby)
                    else:
                        col = elem.evaluate_eager(ee)
                        if expand_dataset and elem.is_reduce_expr(ee):
                            col = col.ungroup(ee.groupby)
                    columns[i] = col
            else:
                columns = [core.expr_column(_dt, e, _ri) if isinstance(e, int)
                           else e.evaluate_eager(ee)
                           for e in self._elems]
            return core.columns_from_columns(columns)

    def execute_update(self):
        raise TValueError("Cannot execute update on computed columns")




#===============================================================================

def make_columnset(arg, ee, new_cols_allowed=False):
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
    """
    dt = ee.dt

    if (arg is None or arg is Ellipsis or
            (isinstance(arg, slice) and arg == slice(None))):
        return AllCSNode(ee)

    if isinstance(arg, (int, str, slice, BaseExpr)):
        if isinstance(arg, bool):
            # Note: True/False are integer objects in Python, however we do
            # not want to treat `df[True]` as the second column in `df`.
            raise TTypeError("A boolean cannot be used as a column selector")

        # Type of the processed column is `U(int, (int, int, int), BaseExpr)`
        pcol = process_column(arg, dt, new_cols_allowed)
        if isinstance(pcol, int):
            return SliceCSNode(ee, pcol, 1, 1)
        elif isinstance(pcol, tuple):
            return SliceCSNode(ee, *pcol)
        elif isinstance(pcol, NewColumnExpr):
            return ArrayCSNode(ee, [-1], [arg])
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

    if isinstance(arg, types.FunctionType):
        res = arg(f)
        return make_columnset(res, ee)

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



def process_column(col, df, new_cols_allowed=False):
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
        if new_cols_allowed:
            try:
                # This raises an exception if `col` cannot be found
                return df.colindex(col)
            except TValueError:
                return NewColumnExpr(col)
        else:
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
